# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload all debug symbols required for crash reporting purposes.

This script need only be used to upload release builds symbols or to debug
crashes on non-release builds (in which case try to only upload the symbols
for those executables involved).
"""

from __future__ import print_function

import ctypes
import functools
import httplib
import multiprocessing
import os
import poster
import random
import socket
import textwrap
import tempfile
import time
import urllib2

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import parallel
from chromite.lib import retry_util
from chromite.scripts import cros_generate_breakpad_symbols


# URLs used for uploading symbols.
OFFICIAL_UPLOAD_URL = 'http://clients2.google.com/cr/symbol'
STAGING_UPLOAD_URL = 'http://clients2.google.com/cr/staging_symbol'


# The crash server rejects files that are this big.
CRASH_SERVER_FILE_LIMIT = 350 * 1024 * 1024
# Give ourselves a little breathing room from what the server expects.
DEFAULT_FILE_LIMIT = CRASH_SERVER_FILE_LIMIT - (10 * 1024 * 1024)


# How long to wait (in seconds) for a single upload to complete.  This has
# to allow for symbols that are up to CRASH_SERVER_FILE_LIMIT in size.
UPLOAD_TIMEOUT = 30 * 60


# Sleep for 200ms in between uploads to avoid DoS'ing symbol server.
DEFAULT_SLEEP_DELAY = 0.2


# Number of seconds to wait before retrying an upload.  The delay will double
# for each subsequent retry of the same symbol file.
INITIAL_RETRY_DELAY = 1

# Allow up to 7 attempts to upload a symbol file (total delay may be
# 1+2+4+8+16+32=63 seconds).
MAX_RETRIES = 6

# Number of total errors, before uploads are no longer attempted.
# This is used to avoid lots of errors causing unreasonable delays.
# See the related, but independent, error values below.
MAX_TOTAL_ERRORS_FOR_RETRY = 30

# A watermark of transient errors which we allow recovery from.  If we hit
# errors infrequently, overall we're probably doing fine.  For example, if
# we have one failure every 100 passes, then we probably don't want to fail
# right away.  But if we hit a string of failures in a row, we want to abort.
#
# The watermark starts at 0 (and can never go below that).  When this error
# level is exceeded, we stop uploading.  When a failure happens, we add the
# fail adjustment, and when an upload succeeds, we add the pass adjustment.
# We want to penalize failures more so that we ramp up when there is a string
# of them, but then slowly back off as things start working.
#
# A quick example:
#  0.0: Starting point.
#  0.0: Upload works, so add -0.5, and then clamp to 0.
#  1.0: Upload fails, so add 1.0.
#  2.0: Upload fails, so add 1.0.
#  1.5: Upload works, so add -0.5.
#  1.0: Upload works, so add -0.5.
ERROR_WATERMARK = 3.0
ERROR_ADJUST_FAIL = 1.0
ERROR_ADJUST_PASS = -0.5


def SymUpload(sym_file, upload_url):
  """Upload a symbol file to a HTTP server

  The upload is a multipart/form-data POST with the following parameters:
    code_file: the basename of the module, e.g. "app"
    code_identifier: the module file's identifier
    debug_file: the basename of the debugging file, e.g. "app"
    debug_identifier: the debug file's identifier, usually consisting of
                      the guid and age embedded in the pdb, e.g.
                      "11111111BBBB3333DDDD555555555555F"
    version: the file version of the module, e.g. "1.2.3.4"
    product: HTTP-friendly product name
    os: the operating system that the module was built for
    cpu: the CPU that the module was built for
    symbol_file: the contents of the breakpad-format symbol file

  Args:
    sym_file: The symbol file to upload
    upload_url: The crash URL to POST the |sym_file| to
  """
  sym_header = cros_generate_breakpad_symbols.ReadSymsHeader(sym_file)

  fields = (
      ('code_file', sym_header.name),
      ('debug_file', sym_header.name),
      ('debug_identifier', sym_header.id.replace('-', '')),
      # Should we set these fields?  They aren't critical, but it might be nice?
      # We'd have to figure out what file this symbol is coming from and what
      # package provides it ...
      #('version', None),
      #('product', 'ChromeOS'),
      ('os', sym_header.os),
      ('cpu', sym_header.cpu),
      poster.encode.MultipartParam.from_file('symbol_file', sym_file),
  )

  data, headers = poster.encode.multipart_encode(fields)
  request = urllib2.Request(upload_url, data, headers)
  request.add_header('User-agent', 'chromite.upload_symbols')
  urllib2.urlopen(request, timeout=UPLOAD_TIMEOUT)


def TestingSymUpload(sym_file, upload_url):
  """A stub version of SymUpload for --testing usage"""
  cmd = ['sym_upload', sym_file, upload_url]
  # Randomly fail 80% of the time (the retry logic makes this 80%/3 per file).
  returncode = random.randint(1, 100) <= 80
  cros_build_lib.Debug('would run (and return %i): %s', returncode,
                       cros_build_lib.CmdToStr(cmd))
  if returncode:
    output = 'Failed to send the symbol file.'
  else:
    output = 'Successfully sent the symbol file.'
  result = cros_build_lib.CommandResult(cmd=cmd, error=None, output=output,
                                        returncode=returncode)
  if returncode:
    exceptions = (
        socket.error('[socket.error] forced test fail'),
        httplib.BadStatusLine('[BadStatusLine] forced test fail'),
        urllib2.HTTPError(upload_url, 400, '[HTTPError] forced test fail',
                          {}, None),
        urllib2.URLError('[URLError] forced test fail'),
    )
    raise random.choice(exceptions)
  else:
    return result


def ErrorLimitHit(num_errors, watermark_errors):
  """See if our error limit has been hit

  Args:
    num_errors: A multiprocessing.Value of the raw number of failures.
    watermark_errors: A multiprocessing.Value of the current rate of failures.

  Returns:
    True if our error limits have been exceeded.
  """
  return ((num_errors is not None and
           num_errors.value > MAX_TOTAL_ERRORS_FOR_RETRY) or
          (watermark_errors is not None and
           watermark_errors.value > ERROR_WATERMARK))


def _UpdateCounter(counter, adj):
  """Update |counter| by |adj|

  Handle atomic updates of |counter|.  Also make sure it does not
  fall below 0.

  Args:
    counter: A multiprocessing.Value to update
    adj: The value to add to |counter|
  """
  def _Update():
    clamp = 0 if type(adj) is int else 0.0
    counter.value = max(clamp, counter.value + adj)

  if hasattr(counter, 'get_lock'):
    with counter.get_lock():
      _Update()
  elif counter is not None:
    _Update()


def UploadSymbol(sym_file, upload_url, file_limit=DEFAULT_FILE_LIMIT,
                 sleep=0, num_errors=None, watermark_errors=None,
                 failed_queue=None):
  """Upload |sym_file| to |upload_url|

  Args:
    sym_file: The full path to the breakpad symbol to upload
    upload_url: The crash server to upload things to
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: Number of seconds to sleep before running
    num_errors: An object to update with the error count (needs a .value member)
    watermark_errors: An object to track current error behavior (needs a .value)
    failed_queue: When a symbol fails, add it to this queue

  Returns:
    The number of errors that were encountered.
  """
  if num_errors is None:
    num_errors = ctypes.c_int()
  if ErrorLimitHit(num_errors, watermark_errors):
    # Abandon ship!  It's on fire!  NOoooooooooooOOOoooooo.
    if failed_queue:
      failed_queue.put(sym_file)
    return 0

  upload_file = sym_file

  if sleep:
    # Keeps us from DoS-ing the symbol server.
    time.sleep(sleep)

  cros_build_lib.Debug('uploading %s' % sym_file)

  # Ideally there'd be a tempfile.SpooledNamedTemporaryFile that we could use.
  with tempfile.NamedTemporaryFile(prefix='upload_symbols',
                                   bufsize=0) as temp_sym_file:
    if file_limit:
      # If the symbols size is too big, strip out the call frame info.  The CFI
      # is unnecessary for 32bit x86 targets where the frame pointer is used (as
      # all of ours have) and it accounts for over half the size of the symbols
      # uploaded.
      file_size = os.path.getsize(sym_file)
      if file_size > file_limit:
        cros_build_lib.Warning('stripping CFI from %s due to size %s > %s',
                               sym_file, file_size, file_limit)
        temp_sym_file.writelines([x for x in open(sym_file, 'rb').readlines()
                                  if not x.startswith('STACK CFI')])
        upload_file = temp_sym_file.name

    # Hopefully the crash server will let it through.  But it probably won't.
    # Not sure what the best answer is in this case.
    file_size = os.path.getsize(upload_file)
    if file_size > CRASH_SERVER_FILE_LIMIT:
      cros_build_lib.PrintBuildbotStepWarnings()
      cros_build_lib.Warning('upload file %s is awfully large, risking '
                             'rejection by the symbol server (%s > %s)',
                             sym_file, file_size, CRASH_SERVER_FILE_LIMIT)

    # Upload the symbol file.
    success = False
    try:
      cros_build_lib.TimedCommand(
          retry_util.RetryException,
          (urllib2.HTTPError, urllib2.URLError), MAX_RETRIES, SymUpload,
          upload_file, upload_url, sleep=INITIAL_RETRY_DELAY,
          timed_log_msg='upload of %10i bytes took %%s: %s' %
                        (file_size, os.path.basename(sym_file)))
      success = True
    except urllib2.HTTPError as e:
      cros_build_lib.Warning('could not upload: %s: HTTP %s: %s',
                             os.path.basename(sym_file), e.code, e.reason)
    except (urllib2.URLError, httplib.HTTPException, socket.error) as e:
      cros_build_lib.Warning('could not upload: %s: %s',
                             os.path.basename(sym_file), e)
    finally:
      if success:
        _UpdateCounter(watermark_errors, ERROR_ADJUST_PASS)
      else:
        _UpdateCounter(num_errors, 1)
        _UpdateCounter(watermark_errors, ERROR_ADJUST_FAIL)
        if failed_queue:
          failed_queue.put(sym_file)

  return num_errors.value


def SymbolFinder(paths):
  """Locate symbol files in |paths|

  Args:
    paths: A list of input paths to walk. Files are returned w/out any checks.
      Dirs are searched for files that end in ".sym".

  Returns:
    Yield every viable sym file.
  """
  for p in paths:
    if os.path.isdir(p):
      for root, _, files in os.walk(p):
        for f in files:
          if f.endswith('.sym'):
            yield os.path.join(root, f)
    else:
      yield p


def UploadSymbols(board=None, official=False, breakpad_dir=None,
                  file_limit=DEFAULT_FILE_LIMIT, sleep=DEFAULT_SLEEP_DELAY,
                  upload_count=None, sym_paths=None, failed_list=None,
                  root=None, retry=True):
  """Upload all the generated symbols for |board| to the crash server

  You can use in a few ways:
    * pass |board| to locate all of its symbols
    * pass |breakpad_dir| to upload all the symbols in there
    * pass |sym_paths| to upload specific symbols (or dirs of symbols)

  Args:
    board: The board whose symbols we wish to upload
    official: Use the official symbol server rather than the staging one
    breakpad_dir: The full path to the breakpad directory where symbols live
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: How long to sleep in between uploads
    upload_count: If set, only upload this many symbols (meant for testing)
    sym_paths: Specific symbol files (or dirs of sym files) to upload,
      otherwise search |breakpad_dir|
    failed_list: Write the names of all sym files we did not upload; can be a
      filename or file-like object.
    root: The tree to prefix to |breakpad_dir| (if |breakpad_dir| is not set)
    retry: Whether we should retry failures.

  Returns:
    The number of errors that were encountered.
  """
  if official:
    upload_url = OFFICIAL_UPLOAD_URL
  else:
    cros_build_lib.Warning('unofficial builds upload to the staging server')
    upload_url = STAGING_UPLOAD_URL

  if sym_paths:
    cros_build_lib.Info('uploading specified symbols to %s', upload_url)
  else:
    if breakpad_dir is None:
      breakpad_dir = os.path.join(
          root,
          cros_generate_breakpad_symbols.FindBreakpadDir(board).lstrip('/'))
    cros_build_lib.Info('uploading all symbols to %s from %s', upload_url,
                        breakpad_dir)
    sym_paths = [breakpad_dir]

  bg_errors = multiprocessing.Value('i')
  watermark_errors = multiprocessing.Value('f')
  failed_queue = multiprocessing.Queue()
  uploader = functools.partial(
      UploadSymbol, file_limit=file_limit, sleep=sleep, num_errors=bg_errors,
      watermark_errors=watermark_errors, failed_queue=failed_queue)

  # For the first run, we collect the symbols that failed.  If the
  # overall failure rate was low, we'll retry them on the second run.
  for retry in (retry, False):
    # We need to limit ourselves to one upload at a time to avoid the server
    # kicking in DoS protection.  See these bugs for more details:
    # http://crbug.com/209442
    # http://crbug.com/212496
    with parallel.BackgroundTaskRunner(uploader, processes=1) as queue:
      for sym_file in SymbolFinder(sym_paths):
        if upload_count == 0:
          break

        queue.put([sym_file, upload_url])

        if upload_count is not None:
          upload_count -= 1
          if upload_count == 0:
            break

    # See if we need to retry, and if we haven't failed too many times already.
    if not retry or ErrorLimitHit(bg_errors, watermark_errors):
      break

    sym_paths = []
    while not failed_queue.empty():
      sym_paths.append(failed_queue.get())
    if sym_paths:
      cros_build_lib.Warning('retrying %i symbols', len(sym_paths))
      if upload_count is not None:
        upload_count += len(sym_paths)
      # Decrement the error count in case we recover in the second pass.
      assert bg_errors.value >= len(sym_paths), 'more failed files than errors?'
      bg_errors.value -= len(sym_paths)
    else:
      # No failed symbols, so just return now.
      break

  # If the user has requested it, save all the symbol files that we failed to
  # upload to a listing file.  This should help with recovery efforts later on.
  if failed_list:
    with cros_build_lib.Open(failed_list, 'wb+') as f:
      while not failed_queue.empty():
        path = failed_queue.get()
        if breakpad_dir:
          path = os.path.relpath(path, breakpad_dir)
        f.write('%s\n' % path)

  return bg_errors.value


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('sym_paths', type='path', nargs='*', default=None)
  parser.add_argument('--board', default=None,
                      help='board to build packages for')
  parser.add_argument('--breakpad_root', type='path', default=None,
                      help='root directory for breakpad symbols')
  parser.add_argument('--official_build', action='store_true', default=False,
                      help='point to official symbol server')
  parser.add_argument('--regenerate', action='store_true', default=False,
                      help='regenerate all symbols')
  parser.add_argument('--upload-count', type=int, default=None,
                      help='only upload # number of symbols')
  parser.add_argument('--strip_cfi', type=int,
                      default=CRASH_SERVER_FILE_LIMIT - (10 * 1024 * 1024),
                      help='strip CFI data for files above this size')
  parser.add_argument('--failed-list', type='path',
                      help='where to save a list of failed symbols')
  parser.add_argument('--testing', action='store_true', default=False,
                      help='run in testing mode')
  parser.add_argument('--yes', action='store_true', default=False,
                      help='answer yes to all prompts')

  opts = parser.parse_args(argv)
  opts.Freeze()

  if opts.sym_paths:
    if opts.regenerate:
      cros_build_lib.Die('--regenerate may not be used with specific files')
  else:
    if opts.board is None:
      cros_build_lib.Die('--board is required')

  if opts.breakpad_root and opts.regenerate:
    cros_build_lib.Die('--regenerate may not be used with --breakpad_root')

  if opts.testing:
    # TODO(build): Kill off --testing mode once unittests are up-to-snuff.
    cros_build_lib.Info('running in testing mode')
    # pylint: disable=W0601,W0603
    global INITIAL_RETRY_DELAY, SymUpload, DEFAULT_SLEEP_DELAY
    INITIAL_RETRY_DELAY = DEFAULT_SLEEP_DELAY = 0
    SymUpload = TestingSymUpload

  if not opts.yes:
    query = textwrap.wrap(textwrap.dedent("""
        Uploading symbols for an entire Chromium OS build is really only
        necessary for release builds and in a few cases for developers
        to debug problems.  It will take considerable time to run.  For
        developer debugging purposes, consider instead passing specific
        files to upload.
    """), 80)
    cros_build_lib.Warning('\n%s', '\n'.join(query))
    if not cros_build_lib.BooleanPrompt(
        prompt='Are you sure you want to upload all build symbols',
        default=False):
      cros_build_lib.Die('better safe than sorry')

  ret = 0
  if opts.regenerate:
    ret += cros_generate_breakpad_symbols.GenerateBreakpadSymbols(
        opts.board, breakpad_dir=opts.breakpad_root)

  ret += UploadSymbols(opts.board, official=opts.official_build,
                       breakpad_dir=opts.breakpad_root,
                       file_limit=opts.strip_cfi, sleep=DEFAULT_SLEEP_DELAY,
                       upload_count=opts.upload_count, sym_paths=opts.sym_paths,
                       failed_list=opts.failed_list)
  if ret:
    cros_build_lib.Error('encountered %i problem(s)', ret)
    # Since exit(status) gets masked, clamp it to 1 so we don't inadvertently
    # return 0 in case we are a multiple of the mask.
    ret = 1

  return ret


# We need this to run once per process.  Do it at module import time as that
# will let us avoid doing it inline at function call time (see SymUpload) as
# that func might be called by the multiprocessing module which means we'll
# do the opener logic multiple times overall.  Plus, if you're importing this
# module, it's a pretty good chance that you're going to need this.
poster.streaminghttp.register_openers()

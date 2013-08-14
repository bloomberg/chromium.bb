# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload all debug symbols required for crash reporting purposes.

This script need only be used to upload release builds symbols or to debug
crashes on non-release builds (in which case try to only upload the symbols
for those executables involved)."""

import ctypes
import multiprocessing
import os
import poster
import random
import textwrap
import tempfile
import time
import urllib2

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import parallel
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

# Number of total errors, TOTAL_ERROR_COUNT, before retries are no longer
# attempted.  This is used to avoid lots of errors causing unreasonable delays.
MAX_TOTAL_ERRORS_FOR_RETRY = 3


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
                       ' '.join(map(repr, cmd)))
  if returncode:
    output = 'Failed to send the symbol file.'
  else:
    output = 'Successfully sent the symbol file.'
  result = cros_build_lib.CommandResult(cmd=cmd, error=None, output=output,
                                        returncode=returncode)
  if returncode:
    raise urllib2.HTTPError(upload_url, 400, 'forced test fail', {}, None)
  else:
    return result


def UploadSymbol(sym_file, upload_url, file_limit=DEFAULT_FILE_LIMIT,
                 sleep=0, num_errors=None):
  """Upload |sym_file| to |upload_url|

  Args:
    sym_file: The full path to the breakpad symbol to upload
    upload_url: The crash server to upload things to
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: Number of seconds to sleep before running
    num_errors: An object to update with the error count (needs a .value member)
  Returns:
    The number of errors that were encountered.
  """
  if num_errors is None:
    num_errors = ctypes.c_int()
  elif num_errors.value > MAX_TOTAL_ERRORS_FOR_RETRY:
    # Abandon ship!  It's on fire!  NOoooooooooooOOOoooooo.
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
      cros_build_lib.Error('upload file %s is awfully large, risking rejection '
                           'by symbol server (%s > %s)', sym_file, file_size,
                           CRASH_SERVER_FILE_LIMIT)
      num_errors.value += 1

    # Upload the symbol file.
    try:
      cros_build_lib.RetryException(urllib2.HTTPError,
                                    MAX_RETRIES, SymUpload, upload_file,
                                    upload_url, sleep=INITIAL_RETRY_DELAY)
      cros_build_lib.Info('successfully uploaded %10i bytes: %s', file_size,
                          os.path.basename(sym_file))
    except urllib2.HTTPError as e:
      cros_build_lib.Warning('could not upload: %s: HTTP %s: %s',
                             os.path.basename(sym_file), e.code, e.reason)
      num_errors.value += 1

  return num_errors.value


def UploadSymbols(board=None, official=False, breakpad_dir=None,
                  file_limit=DEFAULT_FILE_LIMIT, sleep=DEFAULT_SLEEP_DELAY,
                  upload_count=None, sym_files=None):
  """Upload all the generated symbols for |board| to the crash server

  You can use in a few ways:
    * pass |board| to locate all of its symbols
    * pass |breakpad_dir| to upload all the symbols in there
    * pass |sym_files| to upload specific symbols

  Args:
    board: The board whose symbols we wish to upload
    official: Use the official symbol server rather than the staging one
    breakpad_dir: The full path to the breakpad directory where symbols live
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: How long to sleep in between uploads
    upload_count: If set, only upload this many symbols (meant for testing)
    sym_files: Specific symbol files to upload, otherwise search |breakpad_dir|
  Returns:
    The number of errors that were encountered.
  """
  num_errors = 0

  if official:
    upload_url = OFFICIAL_UPLOAD_URL
  else:
    cros_build_lib.Warning('unofficial builds upload to the staging server')
    upload_url = STAGING_UPLOAD_URL

  if sym_files:
    cros_build_lib.Info('uploading specified symbol files to %s', upload_url)
    sym_file_sets = [('', '', sym_files)]
    all_files = True
  else:
    if breakpad_dir is None:
      breakpad_dir = cros_generate_breakpad_symbols.FindBreakpadDir(board)
    cros_build_lib.Info('uploading all symbols to %s from %s', upload_url,
                        breakpad_dir)
    sym_file_sets = os.walk(breakpad_dir)
    all_files = False

  # We need to limit ourselves to one upload at a time to avoid the server
  # kicking in DoS protection.  See these bugs for more details:
  # http://crbug.com/209442
  # http://crbug.com/212496
  bg_errors = multiprocessing.Value('i')
  with parallel.BackgroundTaskRunner(UploadSymbol, file_limit=file_limit,
                                     sleep=sleep, num_errors=bg_errors,
                                     processes=1) as queue:
    for root, _, files in sym_file_sets:
      if upload_count == 0:
        break

      for sym_file in files:
        if all_files or sym_file.endswith('.sym'):
          sym_file = os.path.join(root, sym_file)
          queue.put([sym_file, upload_url])

          if upload_count is not None:
            upload_count -= 1
            if upload_count == 0:
              break
  num_errors += bg_errors.value

  return num_errors


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('sym_files', type='path', nargs='*', default=None)
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
  parser.add_argument('--testing', action='store_true', default=False,
                      help='run in testing mode')
  parser.add_argument('--yes', action='store_true', default=False,
                      help='answer yes to all prompts')

  opts = parser.parse_args(argv)

  if opts.sym_files:
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
                       upload_count=opts.upload_count, sym_files=opts.sym_files)
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

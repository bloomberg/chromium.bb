# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload all debug symbols required for crash reporting purposes.

This script need only be used to upload release builds symbols or to debug
crashes on non-release builds (in which case try to only upload the symbols
for those executables involved).
"""

from __future__ import print_function

import collections
import ctypes
import datetime
import errno
import functools
import hashlib
import httplib
import multiprocessing
import os
import poster
try:
  import Queue
except ImportError:
  # Python-3 renamed to "queue".  We still use Queue to avoid collisions
  # with naming variables as "queue".  Maybe we'll transition at some point.
  # pylint: disable=F0401
  import queue as Queue
import random
import signal
import socket
import sys
import textwrap
import tempfile
import time
import urllib2
import urlparse

from chromite.cbuildbot import constants
from chromite.lib import cache
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import path_util
from chromite.lib import retry_util
from chromite.lib import signals
from chromite.lib import timeout_util
from chromite.scripts import cros_generate_breakpad_symbols

# Needs to be after chromite imports.
# We don't want to import the general keyring module as that will implicitly
# try to import & connect to a dbus server.  That's a waste of time.
sys.modules['keyring'] = None
import isolateserver


# URLs used for uploading symbols.
OFFICIAL_UPLOAD_URL = 'http://clients2.google.com/cr/symbol'
STAGING_UPLOAD_URL = 'http://clients2.google.com/cr/staging_symbol'


# The crash server rejects files that are this big.
CRASH_SERVER_FILE_LIMIT = 350 * 1024 * 1024
# Give ourselves a little breathing room from what the server expects.
DEFAULT_FILE_LIMIT = CRASH_SERVER_FILE_LIMIT - (10 * 1024 * 1024)


# The batch limit when talking to the dedup server.  We avoid sending one at a
# time as the round trip overhead will dominate.  Conversely, we avoid sending
# all at once so we can start uploading symbols asap -- the symbol server is a
# bit slow and will take longer than anything else.
# TODO: A better algorithm would be adaptive.  If we have more than one symbol
# in the upload queue waiting, we could send more symbols to the dedupe server
# at a time.
DEDUPE_LIMIT = 100

# How long to wait for the server to respond with the results.  Note that the
# larger the limit above, the larger this will need to be.  So we give it ~1
# second per item max.
DEDUPE_TIMEOUT = DEDUPE_LIMIT

# How long to wait for the notification to finish (in minutes).  If it takes
# longer than this, we'll stop notifiying, but that's not a big deal as we
# will be able to recover in later runs.
DEDUPE_NOTIFY_TIMEOUT = 20

# The unique namespace in the dedupe server that only we use.  Helps avoid
# collisions with all the hashed values and unrelated content.
OFFICIAL_DEDUPE_NAMESPACE_TMPL = '%s-upload-symbols'
STAGING_DEDUPE_NAMESPACE_TMPL = '%s-staging' % OFFICIAL_DEDUPE_NAMESPACE_TMPL


# The minimum average rate (in bytes per second) that we expect to maintain
# when uploading symbols.  This has to allow for symbols that are up to
# CRASH_SERVER_FILE_LIMIT in size.
UPLOAD_MIN_RATE = CRASH_SERVER_FILE_LIMIT / (30 * 60)

# The lowest timeout (in seconds) we'll allow.  If the server is overloaded,
# then there might be a delay in setting up the connection, not just with the
# transfer.  So even a small file might need a larger value.
UPLOAD_MIN_TIMEOUT = 2 * 60


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


# A named tuple which hold a SymbolItem object and
# a isolateserver._IsolateServerPushState item.
SymbolElement = collections.namedtuple(
    'SymbolElement', ('symbol_item', 'opaque_push_state'))


def GetUploadTimeout(path):
  """How long to wait for a specific file to upload to the crash server.

  This is a function largely to make unittesting easier.

  Args:
    path: The path to the file to calculate the timeout for

  Returns:
    Timeout length (in seconds)
  """
  # Scale the timeout based on the filesize.
  return max(os.path.getsize(path) / UPLOAD_MIN_RATE, UPLOAD_MIN_TIMEOUT)


def SymUpload(upload_url, sym_item, product_name):
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
    upload_url: The crash URL to POST the |sym_file| to
    sym_item: A SymbolItem containing the path to the breakpad symbol to upload
    product_name: A string for stats purposes. Usually 'ChromeOS' or 'Android'.
  """
  sym_header = sym_item.sym_header
  sym_file = sym_item.sym_file

  fields = (
      ('code_file', sym_header.name),
      ('debug_file', sym_header.name),
      ('debug_identifier', sym_header.id.replace('-', '')),
      # The product/version fields are used by the server only for statistic
      # purposes.  They do not impact symbolization, so they're safe to set
      # to any value all the time.
      # In this case, we use it to help see the load our build system is
      # placing on the server.
      # Not sure what to set for the version.  Maybe the git sha1 of this file.
      # Note: the server restricts this to 30 chars.
      #('version', None),
      ('product', product_name),
      ('os', sym_header.os),
      ('cpu', sym_header.cpu),
      poster.encode.MultipartParam.from_file('symbol_file', sym_file),
  )

  data, headers = poster.encode.multipart_encode(fields)
  request = urllib2.Request(upload_url, data, headers)
  request.add_header('User-agent', 'chromite.upload_symbols')
  urllib2.urlopen(request, timeout=GetUploadTimeout(sym_file))


def TestingSymUpload(upload_url, sym_item, _product_name):
  """A stub version of SymUpload for --testing usage"""
  cmd = ['sym_upload', sym_item.sym_file, upload_url]
  # Randomly fail 80% of the time (the retry logic makes this 80%/3 per file).
  returncode = random.randint(1, 100) <= 80
  logging.debug('would run (and return %i): %s', returncode,
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


def UploadSymbol(upload_url, symbol_element, product_name,
                 file_limit=DEFAULT_FILE_LIMIT, sleep=0, num_errors=None,
                 watermark_errors=None, failed_queue=None, passed_queue=None):
  """Upload |sym_element.symbol_item| to |upload_url|

  Args:
    upload_url: The crash server to upload things to
    symbol_element: A SymbolElement tuple. symbol_element.symbol_item is a
                    SymbolItem object containing the path to the breakpad symbol
                    to upload. symbol_element.opaque_push_state is an object of
                    _IsolateServerPushState or None if the item doesn't have
                    a push state.
    product_name: A string for stats purposes. Usually 'ChromeOS' or 'Android'.
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: Number of seconds to sleep before running
    num_errors: An object to update with the error count (needs a .value member)
    watermark_errors: An object to track current error behavior (needs a .value)
    failed_queue: When a symbol fails, add it to this queue
    passed_queue: When a symbol passes, add it to this queue

  Returns:
    The number of errors that were encountered.
  """
  sym_file = symbol_element.symbol_item.sym_file
  upload_item = symbol_element.symbol_item

  if num_errors is None:
    num_errors = ctypes.c_int()
  if ErrorLimitHit(num_errors, watermark_errors):
    # Abandon ship!  It's on fire!  NOoooooooooooOOOoooooo.
    if failed_queue:
      failed_queue.put(sym_file)
    return 0

  if sleep:
    # Keeps us from DoS-ing the symbol server.
    time.sleep(sleep)

  logging.debug('uploading %s' % sym_file)

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
        logging.warning('stripping CFI from %s due to size %s > %s', sym_file,
                        file_size, file_limit)
        temp_sym_file.writelines([x for x in open(sym_file, 'rb').readlines()
                                  if not x.startswith('STACK CFI')])

        upload_item = FakeItem(sym_file=temp_sym_file.name,
                               sym_header=symbol_element.symbol_item.sym_header)

    # Hopefully the crash server will let it through.  But it probably won't.
    # Not sure what the best answer is in this case.
    file_size = os.path.getsize(upload_item.sym_file)
    if file_size > CRASH_SERVER_FILE_LIMIT:
      logging.PrintBuildbotStepWarnings()
      logging.warning('upload file %s is awfully large, risking rejection by '
                      'the symbol server (%s > %s)', sym_file, file_size,
                      CRASH_SERVER_FILE_LIMIT)

    # Upload the symbol file.
    success = False
    try:
      cros_build_lib.TimedCommand(
          retry_util.RetryException,
          (urllib2.HTTPError, urllib2.URLError), MAX_RETRIES, SymUpload,
          upload_url, upload_item, product_name, sleep=INITIAL_RETRY_DELAY,
          timed_log_msg=('upload of %10i bytes took %%(delta)s: %s' %
                         (file_size, os.path.basename(sym_file))))
      success = True

      if passed_queue:
        passed_queue.put(symbol_element)
    except urllib2.HTTPError as e:
      logging.warning('could not upload: %s: HTTP %s: %s',
                      os.path.basename(sym_file), e.code, e.reason)
    except (urllib2.URLError, httplib.HTTPException, socket.error) as e:
      logging.warning('could not upload: %s: %s', os.path.basename(sym_file), e)
    finally:
      if success:
        _UpdateCounter(watermark_errors, ERROR_ADJUST_PASS)
      else:
        _UpdateCounter(num_errors, 1)
        _UpdateCounter(watermark_errors, ERROR_ADJUST_FAIL)
        if failed_queue:
          failed_queue.put(sym_file)

  return num_errors.value


# A dummy class that allows for stubbing in tests and SymUpload.
FakeItem = cros_build_lib.Collection(
    'FakeItem', sym_file=None, sym_header=None, content=lambda x: '')


class SymbolItem(isolateserver.BufferItem):
  """Turn a sym_file into an isolateserver.Item"""

  ALGO = hashlib.sha1

  def __init__(self, sym_file):
    sym_header = cros_generate_breakpad_symbols.ReadSymsHeader(sym_file)
    super(SymbolItem, self).__init__(str(sym_header), self.ALGO)
    self.sym_header = sym_header
    self.sym_file = sym_file


def SymbolDeduplicatorNotify(dedupe_namespace, dedupe_queue):
  """Send a symbol file to the swarming service

  Notify the swarming service of a successful upload.  If the notification fails
  for any reason, we ignore it.  We don't care as it just means we'll upload it
  again later on, and the symbol server will handle that graciously.

  This func runs in a different process from the main one, so we cannot share
  the storage object.  Instead, we create our own.  This func stays alive for
  the life of the process, so we only create one here overall.

  Args:
    dedupe_namespace: The isolateserver namespace to dedupe uploaded symbols.
    dedupe_queue: The queue to read SymbolElements from
  """
  if dedupe_queue is None:
    return

  sym_file = ''
  try:
    with timeout_util.Timeout(DEDUPE_TIMEOUT):
      storage = isolateserver.get_storage_api(constants.ISOLATESERVER,
                                              dedupe_namespace)
    for symbol_element in iter(dedupe_queue.get, None):
      if not symbol_element or not symbol_element.symbol_item:
        continue
      symbol_item = symbol_element.symbol_item
      push_state = symbol_element.opaque_push_state
      sym_file = symbol_item.sym_file if symbol_item.sym_file else ''
      if push_state is not None:
        with timeout_util.Timeout(DEDUPE_TIMEOUT):
          logging.debug('sending %s to dedupe server', sym_file)
          symbol_item.prepare(SymbolItem.ALGO)
          storage.push(symbol_item, push_state, symbol_item.content())
          logging.debug('sent %s', sym_file)
    logging.info('dedupe notification finished; exiting')
  except Exception:
    logging.warning('posting %s to dedupe server failed',
                    os.path.basename(sym_file), exc_info=True)

    # Keep draining the queue though so it doesn't fill up.
    while dedupe_queue.get() is not None:
      continue


def SymbolDeduplicator(storage, sym_paths):
  """Filter out symbol files that we've already uploaded

  Using the swarming service, ask it to tell us which symbol files we've already
  uploaded in previous runs and/or by other bots.  If the query fails for any
  reason, we'll just upload all symbols.  This is fine as the symbol server will
  do the right thing and this phase is purely an optimization.

  This code runs in the main thread which is why we can re-use the existing
  storage object.  Saves us from having to recreate one all the time.

  Args:
    storage: An isolateserver.StorageApi object
    sym_paths: List of symbol files to check against the dedupe server

  Returns:
    List of SymbolElement objects that have not been uploaded before
  """
  if not sym_paths:
    return sym_paths

  items = [SymbolItem(x) for x in sym_paths]
  for item in items:
    item.prepare(SymbolItem.ALGO)
  if storage:
    try:
      with timeout_util.Timeout(DEDUPE_TIMEOUT):
        items = storage.contains(items)
      return [SymbolElement(symbol_item=item, opaque_push_state=push_state)
              for (item, push_state) in items.iteritems()]
    except Exception:
      logging.warning('talking to dedupe server failed', exc_info=True)

  return [SymbolElement(symbol_item=item, opaque_push_state=None)
          for item in items]


def IsTarball(path):
  """Guess if this is a tarball based on the filename."""
  parts = path.split('.')
  if len(parts) <= 1:
    return False

  if parts[-1] == 'tar':
    return True

  if parts[-2] == 'tar':
    return parts[-1] in ('bz2', 'gz', 'xz')

  return parts[-1] in ('tbz2', 'tbz', 'tgz', 'txz')


def SymbolFinder(tempdir, paths):
  """Locate symbol files in |paths|

  Args:
    tempdir: Path to use for temporary files (caller will clean up).
    paths: A list of input paths to walk. Files are returned w/out any checks.
      Dirs are searched for files that end in ".sym". Urls are fetched and then
      processed. Tarballs are unpacked and walked.

  Returns:
    Yield every viable sym file.
  """
  cache_dir = path_util.GetCacheDir()
  common_path = os.path.join(cache_dir, constants.COMMON_CACHE)
  tar_cache = cache.TarballCache(common_path)

  for p in paths:
    # Pylint is confused about members of ParseResult.

    o = urlparse.urlparse(p)
    if o.scheme:  # pylint: disable=E1101
      # Support globs of filenames.
      ctx = gs.GSContext()
      for p in ctx.LS(p):
        logging.info('processing files inside %s', p)
        o = urlparse.urlparse(p)
        key = ('%s%s' % (o.netloc, o.path)).split('/')  # pylint: disable=E1101
        # The common cache will not be LRU, removing the need to hold a read
        # lock on the cached gsutil.
        ref = tar_cache.Lookup(key)
        try:
          ref.SetDefault(p)
        except cros_build_lib.RunCommandError as e:
          logging.warning('ignoring %s\n%s', p, e)
          continue
        for p in SymbolFinder(tempdir, [ref.path]):
          yield p

    elif os.path.isdir(p):
      for root, _, files in os.walk(p):
        for f in files:
          if f.endswith('.sym'):
            yield os.path.join(root, f)

    elif IsTarball(p):
      logging.info('processing files inside %s', p)
      tardir = tempfile.mkdtemp(dir=tempdir)
      cache.Untar(os.path.realpath(p), tardir)
      for p in SymbolFinder(tardir, [tardir]):
        yield p

    else:
      yield p


def WriteQueueToFile(listing, queue, relpath=None):
  """Write all the items in |queue| to the |listing|.

  Note: The queue must have a sentinel None appended to the end.

  Args:
    listing: Where to write out the list of files.
    queue: The queue of paths to drain.
    relpath: If set, write out paths relative to this one.
  """
  if not listing:
    # Still drain the queue so we make sure the producer has finished
    # before we return.  Otherwise, the queue might get destroyed too
    # quickly which will trigger a traceback in the producer.
    while queue.get() is not None:
      continue
    return

  with cros_build_lib.Open(listing, 'wb+') as f:
    while True:
      path = queue.get()
      if path is None:
        return
      if relpath:
        path = os.path.relpath(path, relpath)
      f.write('%s\n' % path)


def UploadSymbols(board=None, official=False, server=None, breakpad_dir=None,
                  file_limit=DEFAULT_FILE_LIMIT, sleep=DEFAULT_SLEEP_DELAY,
                  upload_limit=None, sym_paths=None, failed_list=None,
                  root=None, retry=True, dedupe_namespace=None,
                  product_name='ChromeOS'):
  """Upload all the generated symbols for |board| to the crash server

  You can use in a few ways:
    * pass |board| to locate all of its symbols
    * pass |breakpad_dir| to upload all the symbols in there
    * pass |sym_paths| to upload specific symbols (or dirs of symbols)

  Args:
    board: The board whose symbols we wish to upload
    official: Use the official symbol server rather than the staging one
    server: Explicit server to post symbols to
    breakpad_dir: The full path to the breakpad directory where symbols live
    file_limit: The max file size of a symbol file before we try to strip it
    sleep: How long to sleep in between uploads
    upload_limit: If set, only upload this many symbols (meant for testing)
    sym_paths: Specific symbol files (or dirs of sym files) to upload,
      otherwise search |breakpad_dir|
    failed_list: Write the names of all sym files we did not upload; can be a
      filename or file-like object.
    root: The tree to prefix to |breakpad_dir| (if |breakpad_dir| is not set)
    retry: Whether we should retry failures.
    dedupe_namespace: The isolateserver namespace to dedupe uploaded symbols.
    product_name: A string for stats purposes. Usually 'ChromeOS' or 'Android'.

  Returns:
    The number of errors that were encountered.
  """
  if server is None:
    if official:
      upload_url = OFFICIAL_UPLOAD_URL
    else:
      logging.warning('unofficial builds upload to the staging server')
      upload_url = STAGING_UPLOAD_URL
  else:
    upload_url = server

  if sym_paths:
    logging.info('uploading specified symbols to %s', upload_url)
  else:
    if breakpad_dir is None:
      if root is None:
        raise ValueError('breakpad_dir requires root to be set')
      breakpad_dir = os.path.join(
          root,
          cros_generate_breakpad_symbols.FindBreakpadDir(board).lstrip('/'))
    logging.info('uploading all symbols to %s from %s', upload_url,
                 breakpad_dir)
    sym_paths = [breakpad_dir]

  # We use storage_query to ask the server about existing symbols.  The
  # storage_notify_proc process is used to post updates to the server.  We
  # cannot safely share the storage object between threads/processes, but
  # we also want to minimize creating new ones as each object has to init
  # new state (like server connections).
  storage_query = None
  if dedupe_namespace:
    dedupe_limit = DEDUPE_LIMIT
    dedupe_queue = multiprocessing.Queue()
    try:
      with timeout_util.Timeout(DEDUPE_TIMEOUT):
        storage_query = isolateserver.get_storage_api(constants.ISOLATESERVER,
                                                      dedupe_namespace)
    except Exception:
      logging.warning('initializing dedupe server connection failed',
                      exc_info=True)
  else:
    dedupe_limit = 1
    dedupe_queue = None
  # Can't use parallel.BackgroundTaskRunner because that'll create multiple
  # processes and we want only one the whole time (see comment above).
  storage_notify_proc = multiprocessing.Process(
      target=SymbolDeduplicatorNotify, args=(dedupe_namespace, dedupe_queue))

  bg_errors = multiprocessing.Value('i')
  watermark_errors = multiprocessing.Value('f')
  failed_queue = multiprocessing.Queue()
  uploader = functools.partial(
      UploadSymbol, upload_url, product_name=product_name,
      file_limit=file_limit, sleep=sleep, num_errors=bg_errors,
      watermark_errors=watermark_errors, failed_queue=failed_queue,
      passed_queue=dedupe_queue)

  start_time = datetime.datetime.now()
  Counters = cros_build_lib.Collection(
      'Counters', upload_limit=upload_limit, uploaded_count=0, deduped_count=0)
  counters = Counters()

  def _Upload(queue, counters, files):
    if not files:
      return

    missing_count = 0
    for item in SymbolDeduplicator(storage_query, files):
      missing_count += 1

      if counters.upload_limit == 0:
        continue

      queue.put((item,))
      counters.uploaded_count += 1
      if counters.upload_limit is not None:
        counters.upload_limit -= 1

    counters.deduped_count += (len(files) - missing_count)

  try:
    storage_notify_proc.start()

    with osutils.TempDir(prefix='upload_symbols.') as tempdir:
      # For the first run, we collect the symbols that failed.  If the
      # overall failure rate was low, we'll retry them on the second run.
      for retry in (retry, False):
        # We need to limit ourselves to one upload at a time to avoid the server
        # kicking in DoS protection.  See these bugs for more details:
        # http://crbug.com/209442
        # http://crbug.com/212496
        with parallel.BackgroundTaskRunner(uploader, processes=1) as queue:
          dedupe_list = []
          for sym_file in SymbolFinder(tempdir, sym_paths):
            dedupe_list.append(sym_file)
            dedupe_len = len(dedupe_list)
            if dedupe_len < dedupe_limit:
              if (counters.upload_limit is None or
                  dedupe_len < counters.upload_limit):
                continue

            # We check the counter before _Upload so that we don't keep talking
            # to the dedupe server.  Otherwise, we end up sending one symbol at
            # a time to it and that slows things down a lot.
            if counters.upload_limit == 0:
              break

            _Upload(queue, counters, dedupe_list)
            dedupe_list = []
          _Upload(queue, counters, dedupe_list)

        # See if we need to retry, and if we haven't failed too many times yet.
        if not retry or ErrorLimitHit(bg_errors, watermark_errors):
          break

        sym_paths = []
        failed_queue.put(None)
        while True:
          sym_path = failed_queue.get()
          if sym_path is None:
            break
          sym_paths.append(sym_path)

        if sym_paths:
          logging.warning('retrying %i symbols', len(sym_paths))
          if counters.upload_limit is not None:
            counters.upload_limit += len(sym_paths)
          # Decrement the error count in case we recover in the second pass.
          assert bg_errors.value >= len(sym_paths), \
                 'more failed files than errors?'
          bg_errors.value -= len(sym_paths)
        else:
          # No failed symbols, so just return now.
          break

    # If the user has requested it, save all the symbol files that we failed to
    # upload to a listing file.  This should help with recovery efforts later.
    failed_queue.put(None)
    WriteQueueToFile(failed_list, failed_queue, breakpad_dir)

  finally:
    logging.info('finished uploading; joining background process')
    if dedupe_queue:
      dedupe_queue.put(None)

    # The notification might be slow going, so give it some time to finish.
    # We have to poll here as the process monitor is watching for output and
    # will kill us if we go silent for too long.
    wait_minutes = DEDUPE_NOTIFY_TIMEOUT
    while storage_notify_proc.is_alive() and wait_minutes > 0:
      if dedupe_queue:
        qsize = str(dedupe_queue.qsize())
      else:
        qsize = '[None]'
      logging.info('waiting up to %i minutes for ~%s notifications',
                   wait_minutes, qsize)
      storage_notify_proc.join(60)
      wait_minutes -= 1

    # The process is taking too long, so kill it and complain.
    if storage_notify_proc.is_alive():
      logging.warning('notification process took too long')
      logging.PrintBuildbotStepWarnings()

      # Kill it gracefully first (traceback) before tacking it down harder.
      pid = storage_notify_proc.pid
      for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGKILL):
        logging.warning('sending %s to %i', signals.StrSignal(sig), pid)
        # The process might have exited between the last check and the
        # actual kill below, so ignore ESRCH errors.
        try:
          os.kill(pid, sig)
        except OSError as e:
          if e.errno == errno.ESRCH:
            break
          else:
            raise
        time.sleep(5)
        if not storage_notify_proc.is_alive():
          break

      # Drain the queue so we don't hang when we finish.
      try:
        logging.warning('draining the notify queue manually')
        with timeout_util.Timeout(60):
          try:
            while dedupe_queue.get_nowait():
              pass
          except Queue.Empty:
            pass
      except timeout_util.TimeoutError:
        logging.warning('draining the notify queue failed; trashing it')
        dedupe_queue.cancel_join_thread()

  logging.info('uploaded %i symbols (%i were deduped) which took: %s',
               counters.uploaded_count, counters.deduped_count,
               datetime.datetime.now() - start_time)

  return bg_errors.value


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('sym_paths', type='path_or_uri', nargs='*', default=None,
                      help='symbol file or directory or URL or tarball')
  parser.add_argument('--board', default=None,
                      help='Used to find default breakpad_root.')
  parser.add_argument('--breakpad_root', type='path', default=None,
                      help='full path to the breakpad symbol directory')
  parser.add_argument('--root', type='path', default=None,
                      help='full path to the chroot dir')
  parser.add_argument('--official_build', action='store_true', default=False,
                      help='point to official symbol server')
  parser.add_argument('--server', type=str, default=None,
                      help='URI for custom symbol server')
  parser.add_argument('--regenerate', action='store_true', default=False,
                      help='regenerate all symbols')
  parser.add_argument('--upload-limit', type=int, default=None,
                      help='only upload # number of symbols')
  parser.add_argument('--strip_cfi', type=int,
                      default=CRASH_SERVER_FILE_LIMIT - (10 * 1024 * 1024),
                      help='strip CFI data for files above this size')
  parser.add_argument('--failed-list', type='path',
                      help='where to save a list of failed symbols')
  parser.add_argument('--dedupe', action='store_true', default=False,
                      help='use the swarming service to avoid re-uploading')
  parser.add_argument('--testing', action='store_true', default=False,
                      help='run in testing mode')
  parser.add_argument('--yes', action='store_true', default=False,
                      help='answer yes to all prompts')
  parser.add_argument('--product_name', type=str, default='ChromeOS',
                      help='Produce Name for breakpad stats.')

  opts = parser.parse_args(argv)
  opts.Freeze()

  if opts.sym_paths or opts.breakpad_root:
    if opts.regenerate:
      cros_build_lib.Die('--regenerate may not be used with specific files, '
                         'or breakpad_root')
  else:
    if opts.board is None:
      cros_build_lib.Die('--board is required')

  if opts.testing:
    # TODO(build): Kill off --testing mode once unittests are up-to-snuff.
    logging.info('running in testing mode')
    # pylint: disable=W0601,W0603
    global INITIAL_RETRY_DELAY, SymUpload, DEFAULT_SLEEP_DELAY
    INITIAL_RETRY_DELAY = DEFAULT_SLEEP_DELAY = 0
    SymUpload = TestingSymUpload

  dedupe_namespace = None
  if opts.dedupe:
    if opts.official_build and not opts.testing:
      dedupe_namespace = OFFICIAL_DEDUPE_NAMESPACE_TMPL % opts.product_name
    else:
      dedupe_namespace = STAGING_DEDUPE_NAMESPACE_TMPL % opts.product_name

  if not opts.yes:
    prolog = '\n'.join(textwrap.wrap(textwrap.dedent("""
        Uploading symbols for an entire Chromium OS build is really only
        necessary for release builds and in a few cases for developers
        to debug problems.  It will take considerable time to run.  For
        developer debugging purposes, consider instead passing specific
        files to upload.
    """), 80)).strip()
    if not cros_build_lib.BooleanPrompt(
        prompt='Are you sure you want to upload all build symbols',
        default=False, prolog=prolog):
      cros_build_lib.Die('better safe than sorry')

  ret = 0
  if opts.regenerate:
    ret += cros_generate_breakpad_symbols.GenerateBreakpadSymbols(
        opts.board, breakpad_dir=opts.breakpad_root)

  ret += UploadSymbols(opts.board, official=opts.official_build,
                       server=opts.server, breakpad_dir=opts.breakpad_root,
                       file_limit=opts.strip_cfi, sleep=DEFAULT_SLEEP_DELAY,
                       upload_limit=opts.upload_limit, sym_paths=opts.sym_paths,
                       failed_list=opts.failed_list, root=opts.root,
                       dedupe_namespace=dedupe_namespace,
                       product_name=opts.product_name)
  if ret:
    logging.error('encountered %i problem(s)', ret)
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

# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for upload_symbols.py"""

from __future__ import print_function

import errno
import itertools
import os
import signal
import socket
import sys
import time

import mock
from six.moves import BaseHTTPServer
from six.moves import socketserver
from six.moves import urllib

# We specifically set up a local server to connect to, so make sure we
# delete any proxy settings that might screw that up.  We also need to
# do it here because modules that are imported below will implicitly
# initialize with this proxy setting rather than dynamically pull it
# on the fly :(.
# pylint: disable=wrong-import-position
os.environ.pop('http_proxy', None)

from chromite.lib import constants

# The isolateserver includes a bunch of third_party python packages that clash
# with chromite's bundled third_party python packages (like oauth2client).
# Since upload_symbols is not imported in to other parts of chromite, and there
# are no deps in third_party we care about, purge the chromite copy.  This way
# we can use isolateserver for deduping.
# TODO: If we ever sort out third_party/ handling and make it per-script opt-in,
# we can purge this logic.
third_party = os.path.join(constants.CHROMITE_DIR, 'third_party')
while True:
  try:
    sys.path.remove(third_party)
  except ValueError:
    break
del third_party

from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import remote_access
from chromite.scripts import cros_generate_breakpad_symbols
from chromite.scripts import upload_symbols


class SymbolsTestBase(cros_test_lib.MockTempDirTestCase):
  """Base class for most symbols tests."""

  SLIM_CONTENT = """
some junk
"""

  FAT_CONTENT = """
STACK CFI 1234
some junk
STACK CFI 1234
"""

  def setUp(self):
    # Make certain we don't use the network.
    self.urlopen_mock = self.PatchObject(urllib.request, 'urlopen')
    self.request_mock = self.PatchObject(upload_symbols, 'ExecRequest',
                                         return_value={'uploadUrl':
                                                       'testurl',
                                                       'uploadKey':
                                                       'asdgasgas'})

    # Make 'uploads' go fast.
    self.PatchObject(upload_symbols, 'SLEEP_DELAY', 0)
    self.PatchObject(upload_symbols, 'INITIAL_RETRY_DELAY', 0)

    # So our symbol file content doesn't have to be real.
    self.PatchObject(cros_generate_breakpad_symbols, 'ReadSymsHeader',
                     return_value=cros_generate_breakpad_symbols.SymbolHeader(
                         os='os', cpu='cpu', id='id', name='name'))

    self.working = os.path.join(self.tempdir, 'expand')
    osutils.SafeMakedirs(self.working)

    self.data = os.path.join(self.tempdir, 'data')
    osutils.SafeMakedirs(self.data)

  def createSymbolFile(self, filename, content=FAT_CONTENT, size=0,
                       status=None, dedupe=False):
    fullname = os.path.join(self.data, filename)
    osutils.SafeMakedirs(os.path.dirname(fullname))

    # If a file size is given, force that to be the minimum file size. Create
    # a sparse file so large files are practical.
    with open(fullname, 'w+b') as f:
      f.truncate(size)
      f.seek(0)
      f.write(content)

    result = upload_symbols.SymbolFile(display_path=filename,
                                       file_name=fullname)

    if status:
      result.status = status

    if dedupe:
      result.dedupe_item = upload_symbols.DedupeItem(result)
      result.dedupe_push_state = 'push_state'

    return result


class SymbolServerRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """HTTP handler for symbol POSTs"""

  RESP_CODE = None
  RESP_MSG = None

  def do_POST(self):
    """Handle a POST request"""
    # Drain the data from the client.  If we don't, we might write the response
    # and close the socket before the client finishes, so they die with EPIPE.
    clen = int(self.headers.get('Content-Length', '0'))
    self.rfile.read(clen)

    self.send_response(self.RESP_CODE, self.RESP_MSG)
    self.end_headers()

  def log_message(self, *args, **kwargs):
    """Stub the logger as it writes to stderr"""
    pass


class SymbolServer(socketserver.ThreadingTCPServer, BaseHTTPServer.HTTPServer):
  """Simple HTTP server that forks each request"""


class UploadSymbolsServerTest(cros_test_lib.MockTempDirTestCase):
  """Tests for UploadSymbols() and a local HTTP server"""

  SYM_CONTENTS = """MODULE Linux arm 123-456 blkid
PUBLIC 1471 0 main"""

  def SpawnServer(self, RequestHandler):
    """Spawn a new http server"""
    while True:
      try:
        port = remote_access.GetUnusedPort()
        address = ('', port)
        self.httpd = SymbolServer(address, RequestHandler)
        break
      except socket.error as e:
        if e.errno == errno.EADDRINUSE:
          continue
        raise
    self.server_url = 'http://localhost:%i/post/path' % port
    self.httpd_pid = os.fork()
    if self.httpd_pid == 0:
      self.httpd.serve_forever(poll_interval=0.1)
      sys.exit(0)

  def setUp(self):
    self.httpd_pid = None
    self.httpd = None
    self.server_url = None
    self.sym_file = os.path.join(self.tempdir, 'test.sym')
    osutils.WriteFile(self.sym_file, self.SYM_CONTENTS)

    # Stop sleeps and retries for these tests.
    self.PatchObject(upload_symbols, 'SLEEP_DELAY', 0)
    self.PatchObject(upload_symbols, 'INITIAL_RETRY_DELAY', 0)
    self.PatchObject(upload_symbols, 'MAX_RETRIES', 0)

  def tearDown(self):
    # Only kill the server if we forked one.
    if self.httpd_pid:
      os.kill(self.httpd_pid, signal.SIGUSR1)

  def testSuccess(self):
    """The server returns success for all uploads"""
    class Handler(SymbolServerRequestHandler):
      """Always return 200"""
      RESP_CODE = 200
      self.PatchObject(upload_symbols, 'ExecRequest',
                       return_value={'uploadUrl': 'testurl',
                                     'uploadKey': 'testSuccess'})
    self.SpawnServer(Handler)
    ret = upload_symbols.UploadSymbols(
        sym_paths=[self.sym_file] * 10,
        upload_url=self.server_url,
        api_key='testSuccess')
    self.assertEqual(ret, 0)

  def testError(self):
    """The server returns errors for all uploads"""
    class Handler(SymbolServerRequestHandler):
      """All connections error"""
      RESP_CODE = 500
      RESP_MSG = 'Internal Server Error'

    self.SpawnServer(Handler)
    ret = upload_symbols.UploadSymbols(
        sym_paths=[self.sym_file] * 10,
        upload_url=self.server_url,
        api_key='testkey')
    self.assertEqual(ret, 10)

  def testHungServer(self):
    """The server chokes, but we recover"""
    class Handler(SymbolServerRequestHandler):
      """All connections choke forever"""
      self.PatchObject(upload_symbols, 'ExecRequest',
                       return_value={'pairs': []})

      def do_POST(self):
        while True:
          time.sleep(1000)

    self.SpawnServer(Handler)
    with mock.patch.object(upload_symbols, 'GetUploadTimeout') as m:
      m.return_value = 0.01
      ret = upload_symbols.UploadSymbols(
          sym_paths=[self.sym_file] * 10,
          upload_url=self.server_url,
          timeout=m.return_value,
          api_key='testkey')
    self.assertEqual(ret, 10)


class UploadSymbolsHelpersTest(cros_test_lib.TestCase):
  """Test assorted helper functions and classes."""
  def testIsTarball(self):
    notTar = [
        '/foo/bar/test.bin',
        '/foo/bar/test.tar.bin',
        '/foo/bar/test.faketar.gz',
        '/foo/bar/test.nottgz',
    ]

    isTar = [
        '/foo/bar/test.tar',
        '/foo/bar/test.bin.tar',
        '/foo/bar/test.bin.tar.bz2',
        '/foo/bar/test.bin.tar.gz',
        '/foo/bar/test.bin.tar.xz',
        '/foo/bar/test.tbz2',
        '/foo/bar/test.tbz',
        '/foo/bar/test.tgz',
        '/foo/bar/test.txz',
    ]

    for p in notTar:
      self.assertFalse(upload_symbols.IsTarball(p))

    for p in isTar:
      self.assertTrue(upload_symbols.IsTarball(p))

  def testBatchGenerator(self):
    result = upload_symbols.BatchGenerator([], 2)
    self.assertEqual(list(result), [])

    # BatchGenerator accepts iterators, so passing it here is safe.
    # pylint: disable=range-builtin-not-iterating
    result = upload_symbols.BatchGenerator(range(6), 2)
    self.assertEqual(list(result), [[0, 1], [2, 3], [4, 5]])

    result = upload_symbols.BatchGenerator(range(7), 2)
    self.assertEqual(list(result), [[0, 1], [2, 3], [4, 5], [6]])

    # Prove that we are streaming the results, not generating them all at once.
    result = upload_symbols.BatchGenerator(itertools.repeat(0), 2)
    self.assertEqual(next(result), [0, 0])


class FindSymbolFilesTest(SymbolsTestBase):
  """Test FindSymbolFiles."""
  def setUp(self):
    self.symfile = self.createSymbolFile('root.sym').file_name
    self.innerfile = self.createSymbolFile(
        os.path.join('nested', 'inner.sym')).file_name

    # CreateTarball is having issues outside the chroot from open file tests.
    #
    # self.tarball = os.path.join(self.tempdir, 'syms.tar.gz')
    # cros_build_lib.CreateTarball(
    #     'syms.tar.gz', self.tempdir, inputs=(self.data))

  def testEmpty(self):
    symbols = list(upload_symbols.FindSymbolFiles(
        self.working, []))
    self.assertEqual(symbols, [])

  def testFile(self):
    symbols = list(upload_symbols.FindSymbolFiles(
        self.working, [self.symfile]))

    self.assertEqual(len(symbols), 1)
    sf = symbols[0]

    self.assertEqual(sf.display_name, 'root.sym')
    self.assertEqual(sf.display_path, self.symfile)
    self.assertEqual(sf.file_name, self.symfile)
    self.assertEqual(sf.status, upload_symbols.SymbolFile.INITIAL)
    self.assertEqual(sf.FileSize(), len(self.FAT_CONTENT))

  def testDir(self):
    symbols = list(upload_symbols.FindSymbolFiles(
        self.working, [self.data]))

    self.assertEqual(len(symbols), 2)
    root = symbols[0]
    nested = symbols[1]

    self.assertEqual(root.display_name, 'root.sym')
    self.assertEqual(root.display_path, 'root.sym')
    self.assertEqual(root.file_name, self.symfile)
    self.assertEqual(root.status, upload_symbols.SymbolFile.INITIAL)
    self.assertEqual(root.FileSize(), len(self.FAT_CONTENT))

    self.assertEqual(nested.display_name, 'inner.sym')
    self.assertEqual(nested.display_path, 'nested/inner.sym')
    self.assertEqual(nested.file_name, self.innerfile)
    self.assertEqual(nested.status, upload_symbols.SymbolFile.INITIAL)
    self.assertEqual(nested.FileSize(), len(self.FAT_CONTENT))


class AdjustSymbolFileSizeTest(SymbolsTestBase):
  """Test AdjustSymbolFileSize."""
  def setUp(self):
    self.slim = self.createSymbolFile('slim.sym', self.SLIM_CONTENT)
    self.fat = self.createSymbolFile('fat.sym', self.FAT_CONTENT)

    self.warn_mock = self.PatchObject(logging, 'PrintBuildbotStepWarnings')

  def _testNotStripped(self, symbol, size=None, content=None):
    start_file = symbol.file_name
    after = upload_symbols.AdjustSymbolFileSize(
        symbol, self.working, size)
    self.assertIs(after, symbol)
    self.assertEqual(after.file_name, start_file)
    if content is not None:
      self.assertEqual(osutils.ReadFile(after.file_name), content)

  def _testStripped(self, symbol, size=None, content=None):
    after = upload_symbols.AdjustSymbolFileSize(
        symbol, self.working, size)
    self.assertIs(after, symbol)
    self.assertTrue(after.file_name.startswith(self.working))
    if content is not None:
      self.assertEqual(osutils.ReadFile(after.file_name), content)

  def testSmall(self):
    """Ensure that files smaller than the limit are not modified."""
    self._testNotStripped(self.slim, 1024, self.SLIM_CONTENT)
    self._testNotStripped(self.fat, 1024, self.FAT_CONTENT)

  def testLarge(self):
    """Ensure that files larger than the limit are modified."""
    self._testStripped(self.slim, 1, self.SLIM_CONTENT)
    self._testStripped(self.fat, 1, self.SLIM_CONTENT)

  def testMixed(self):
    """Test mix of large and small."""
    strip_size = len(self.SLIM_CONTENT) + 1

    self._testNotStripped(self.slim, strip_size, self.SLIM_CONTENT)
    self._testStripped(self.fat, strip_size, self.SLIM_CONTENT)

  def testSizeWarnings(self):
    large = self.createSymbolFile(
        'large.sym', content=self.SLIM_CONTENT,
        size=upload_symbols.CRASH_SERVER_FILE_LIMIT*2)

    # Would like to Strip as part of this test, but that really copies all
    # of the sparse file content, which is too expensive for a unittest.
    self._testNotStripped(large, None, None)

    self.assertEqual(self.warn_mock.call_count, 1)


class DeduplicateTest(SymbolsTestBase):
  """Test server Deduplication."""
  def setUp(self):
    self.PatchObject(upload_symbols, 'ExecRequest',
                     return_value={'pairs': [
                         {'status': 'FOUND',
                          'symbolId':
                          {'debugFile': 'sym1_sym',
                           'debugId': 'BEAA9BE'}},
                         {'status': 'FOUND',
                          'symbolId':
                          {'debugFile': 'sym2_sym',
                           'debugId': 'B6B1A36'}},
                         {'status': 'MISSING',
                          'symbolId':
                          {'debugFile': 'sym3_sym',
                           'debugId': 'D4FC0FC'}}]})

  def testFindDuplicates(self):
    # The first two symbols will be duplicate, the third new.
    sym1 = self.createSymbolFile('sym1.sym')
    sym1.header = cros_generate_breakpad_symbols.SymbolHeader('cpu', 'BEAA9BE',
                                                              'sym1_sym', 'os')
    sym2 = self.createSymbolFile('sym2.sym')
    sym2.header = cros_generate_breakpad_symbols.SymbolHeader('cpu', 'B6B1A36',
                                                              'sym2_sym', 'os')
    sym3 = self.createSymbolFile('sym3.sym')
    sym3.header = cros_generate_breakpad_symbols.SymbolHeader('cpu', 'D4FC0FC',
                                                              'sym3_sym', 'os')

    result = upload_symbols.FindDuplicates((sym1, sym2, sym3), 'fake_url',
                                           api_key='testkey')
    self.assertEqual(list(result), [sym1, sym2, sym3])

    self.assertEqual(sym1.status, upload_symbols.SymbolFile.DUPLICATE)
    self.assertEqual(sym2.status, upload_symbols.SymbolFile.DUPLICATE)
    self.assertEqual(sym3.status, upload_symbols.SymbolFile.INITIAL)


class PerformSymbolFilesUploadTest(SymbolsTestBase):
  """Test PerformSymbolFile, and it's helper methods."""
  def setUp(self):
    self.sym_initial = self.createSymbolFile(
        'initial.sym')
    self.sym_error = self.createSymbolFile(
        'error.sym', status=upload_symbols.SymbolFile.ERROR)
    self.sym_duplicate = self.createSymbolFile(
        'duplicate.sym', status=upload_symbols.SymbolFile.DUPLICATE)
    self.sym_uploaded = self.createSymbolFile(
        'uploaded.sym', status=upload_symbols.SymbolFile.UPLOADED)

  def testGetUploadTimeout(self):
    """Test GetUploadTimeout helper function."""
    # Timeout for small file.
    self.assertEqual(upload_symbols.GetUploadTimeout(self.sym_initial),
                     upload_symbols.UPLOAD_MIN_TIMEOUT)

    # Timeout for 300M file.
    large = self.createSymbolFile('large.sym', size=(300 * 1024 * 1024))
    self.assertEqual(upload_symbols.GetUploadTimeout(large), 771)

  def testUploadSymbolFile(self):
    upload_symbols.UploadSymbolFile('fake_url', self.sym_initial,
                                    api_key='testkey')
    # TODO: Examine mock in more detail to make sure request is correct.
    self.assertEqual(self.request_mock.call_count, 3)

  def testPerformSymbolsFileUpload(self):
    """We upload on first try."""
    symbols = [self.sym_initial]

    result = upload_symbols.PerformSymbolsFileUpload(
        symbols, 'fake_url', api_key='testkey')

    self.assertEqual(list(result), symbols)
    self.assertEqual(self.sym_initial.status,
                     upload_symbols.SymbolFile.UPLOADED)
    self.assertEqual(self.request_mock.call_count, 3)

  def testPerformSymbolsFileUploadFailure(self):
    """All network requests fail."""
    self.request_mock.side_effect = IOError('network failure')
    symbols = [self.sym_initial]

    result = upload_symbols.PerformSymbolsFileUpload(
        symbols, 'fake_url', api_key='testkey')

    self.assertEqual(list(result), symbols)
    self.assertEqual(self.sym_initial.status, upload_symbols.SymbolFile.ERROR)
    self.assertEqual(self.request_mock.call_count, 7)

  def testPerformSymbolsFileUploadTransisentFailure(self):
    """We fail once, then succeed."""
    self.urlopen_mock.side_effect = (IOError('network failure'), None)
    symbols = [self.sym_initial]

    result = upload_symbols.PerformSymbolsFileUpload(
        symbols, 'fake_url', api_key='testkey')

    self.assertEqual(list(result), symbols)
    self.assertEqual(self.sym_initial.status,
                     upload_symbols.SymbolFile.UPLOADED)
    self.assertEqual(self.request_mock.call_count, 3)

  def testPerformSymbolsFileUploadMixed(self):
    """Upload symbols in mixed starting states.

    Demonstrate that INITIAL and ERROR are uploaded, but DUPLICATE/UPLOADED are
    ignored.
    """
    symbols = [self.sym_initial, self.sym_error,
               self.sym_duplicate, self.sym_uploaded]

    result = upload_symbols.PerformSymbolsFileUpload(
        symbols, 'fake_url', api_key='testkey')

    #
    self.assertEqual(list(result), symbols)
    self.assertEqual(self.sym_initial.status,
                     upload_symbols.SymbolFile.UPLOADED)
    self.assertEqual(self.sym_error.status,
                     upload_symbols.SymbolFile.UPLOADED)
    self.assertEqual(self.sym_duplicate.status,
                     upload_symbols.SymbolFile.DUPLICATE)
    self.assertEqual(self.sym_uploaded.status,
                     upload_symbols.SymbolFile.UPLOADED)
    self.assertEqual(self.request_mock.call_count, 6)


  def testPerformSymbolsFileUploadErrorOut(self):
    """Demonstate we exit only after X errors."""

    symbol_count = upload_symbols.MAX_TOTAL_ERRORS_FOR_RETRY + 10
    symbols = []
    fail_file = None

    # potentially twice as many errors as we should attempt.
    for _ in range(symbol_count):
      # Each loop will get unique SymbolFile instances that use the same files.
      fail = self.createSymbolFile('fail.sym')
      fail_file = fail.file_name
      symbols.append(self.createSymbolFile('pass.sym'))
      symbols.append(fail)

    # Mock out UploadSymbolFile and fail for fail.sym files.
    def failSome(_url, symbol, _api_key):
      if symbol.file_name == fail_file:
        raise IOError('network failure')

    upload_mock = self.PatchObject(upload_symbols, 'UploadSymbolFile',
                                   side_effect=failSome)
    upload_mock.__name__ = 'UploadSymbolFileMock2'

    result = upload_symbols.PerformSymbolsFileUpload(
        symbols, 'fake_url', api_key='testkey')

    self.assertEqual(list(result), symbols)

    passed = sum(s.status == upload_symbols.SymbolFile.UPLOADED
                 for s in symbols)
    failed = sum(s.status == upload_symbols.SymbolFile.ERROR
                 for s in symbols)
    skipped = sum(s.status == upload_symbols.SymbolFile.INITIAL
                  for s in symbols)

    # Shows we all pass.sym files worked until limit hit.
    self.assertEqual(passed, upload_symbols.MAX_TOTAL_ERRORS_FOR_RETRY)

    # Shows we all fail.sym files failed until limit hit.
    self.assertEqual(failed, upload_symbols.MAX_TOTAL_ERRORS_FOR_RETRY)

    # Shows both pass/fail were skipped after limit hit.
    self.assertEqual(skipped, 10 * 2)


class UploadSymbolsTest(SymbolsTestBase):
  """Test UploadSymbols, along with most helper methods."""
  def setUp(self):
    # Results gathering.
    self.failure_file = os.path.join(self.tempdir, 'failures.txt')

  def testUploadSymbolsEmpty(self):
    """Upload dir is empty."""
    result = upload_symbols.UploadSymbols([self.data], 'fake_url')

    self.assertEqual(result, 0)
    self.assertEqual(self.urlopen_mock.call_count, 0)

  def testUploadSymbols(self):
    """Upload a few files."""
    self.createSymbolFile('slim.sym', self.SLIM_CONTENT)
    self.createSymbolFile(os.path.join('nested', 'inner.sym'))
    self.createSymbolFile('fat.sym', self.FAT_CONTENT)

    result = upload_symbols.UploadSymbols(
        [self.data], 'fake_url',
        failed_list=self.failure_file, strip_cfi=len(self.SLIM_CONTENT)+1,
        api_key='testkey')

    self.assertEqual(result, 0)
    self.assertEqual(self.request_mock.call_count, 10)
    self.assertEqual(osutils.ReadFile(self.failure_file), '')

  def testUploadSymbolsLimited(self):
    """Upload a few files."""
    self.createSymbolFile('slim.sym', self.SLIM_CONTENT)
    self.createSymbolFile(os.path.join('nested', 'inner.sym'))
    self.createSymbolFile('fat.sym', self.FAT_CONTENT)

    result = upload_symbols.UploadSymbols(
        [self.data], 'fake_url', upload_limit=2,
        api_key='testkey')

    self.assertEqual(result, 0)
    self.assertEqual(self.request_mock.call_count, 7)
    self.assertNotExists(self.failure_file)

  def testUploadSymbolsFailures(self):
    """Upload a few files."""
    self.createSymbolFile('pass.sym')
    fail = self.createSymbolFile('fail.sym')

    def failSome(_url, symbol, _api_key):
      if symbol.file_name == fail.file_name:
        raise IOError('network failure')

    # Mock out UploadSymbolFile so it's easy to see which file to fail for.
    upload_mock = self.PatchObject(upload_symbols, 'UploadSymbolFile',
                                   side_effect=failSome)
    # Mock __name__ for logging.
    upload_mock.__name__ = 'UploadSymbolFileMock'

    result = upload_symbols.UploadSymbols(
        [self.data], 'fake_url',
        failed_list=self.failure_file, api_key='testkey')

    self.assertEqual(result, 1)
    self.assertEqual(upload_mock.call_count, 8)
    self.assertEqual(osutils.ReadFile(self.failure_file), 'fail.sym\n')

# TODO: We removed --network integration tests.


def main(_argv):
  # pylint: disable=protected-access
  # Set timeouts small so that if the unit test hangs, it won't hang for long.
  parallel._BackgroundTask.STARTUP_TIMEOUT = 5
  parallel._BackgroundTask.EXIT_TIMEOUT = 5

  # Run the tests.
  cros_test_lib.main(level='info', module=__name__)

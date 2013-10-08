#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=W0212
# pylint: disable=W0223
# pylint: disable=W0231

import hashlib
import json
import logging
import os
import shutil
import StringIO
import sys
import tempfile
import threading
import unittest
import urllib
import zlib

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_PATH)
sys.path.insert(0, ROOT_DIR)

import auto_stub
import isolateserver

from utils import threading_utils


ALGO = hashlib.sha1


class TestCase(auto_stub.TestCase):
  def setUp(self):
    super(TestCase, self).setUp()
    self.mock(isolateserver.net, 'url_open', self._url_open)
    self.mock(isolateserver.net, 'sleep_before_retry', lambda *_: None)
    self._lock = threading.Lock()
    self._requests = []

  def tearDown(self):
    try:
      self.assertEqual([], self._requests)
    finally:
      super(TestCase, self).tearDown()

  def _url_open(self, url, **kwargs):
    logging.warn('url_open(%s, %s)', url[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      # Ignore 'stream' argument, it's not important for these tests.
      kwargs.pop('stream', None)
      for i, n in enumerate(self._requests):
        if n[0] == url:
          _, expected_kwargs, result = self._requests.pop(i)
          self.assertEqual(expected_kwargs, kwargs)
          if result is not None:
            return isolateserver.net.HttpResponse.get_fake_response(result, url)
          return None
    self.fail('Unknown request %s' % url)


class TestZipCompression(TestCase):
  """Test zip_compress and zip_decompress generators."""

  def test_compress_and_decompress(self):
    """Test data === decompress(compress(data))."""
    original = [str(x) for x in xrange(0, 1000)]
    processed = isolateserver.zip_decompress(
        isolateserver.zip_compress(original))
    self.assertEqual(''.join(original), ''.join(processed))

  def test_zip_bomb(self):
    """Verify zip_decompress always returns small chunks."""
    original = '\x00' * 100000
    bomb = ''.join(isolateserver.zip_compress(original))
    decompressed = []
    chunk_size = 1000
    for chunk in isolateserver.zip_decompress([bomb], chunk_size):
      self.assertLessEqual(len(chunk), chunk_size)
      decompressed.append(chunk)
    self.assertEqual(original, ''.join(decompressed))

  def test_bad_zip_file(self):
    """Verify decompressing broken file raises IOError."""
    with self.assertRaises(IOError):
      ''.join(isolateserver.zip_decompress(['Im not a zip file']))


class FakeItem(isolateserver.Item):
  def __init__(self, data, is_isolated=False):
    super(FakeItem, self).__init__(
        ALGO(data).hexdigest(), len(data), is_isolated)
    self.data = data

  def content(self, _chunk_size):
    return [self.data]

  @property
  def zipped(self):
    return zlib.compress(self.data, self.compression_level)


class StorageTest(TestCase):
  """Tests for Storage methods."""

  @staticmethod
  def mock_push(side_effect=None):
    """Returns StorageApi subclass with mocked 'push' method."""
    class MockedStorageApi(isolateserver.StorageApi):
      def __init__(self):
        self.pushed = []
      def push(self, item, content):
        self.pushed.append((item, ''.join(content)))
        if side_effect:
          side_effect()
    return MockedStorageApi()

  def assertEqualIgnoringOrder(self, a, b):
    """Asserts that containers |a| and |b| contain same items."""
    self.assertEqual(len(a), len(b))
    self.assertEqual(set(a), set(b))

  def test_batch_items_for_check(self):
    items = [
      isolateserver.Item('foo', 12),
      isolateserver.Item('blow', 0),
      isolateserver.Item('bizz', 1222),
      isolateserver.Item('buzz', 1223),
    ]
    expected = [
      [items[3], items[2], items[0], items[1]],
    ]
    batches = list(isolateserver.Storage.batch_items_for_check(items))
    self.assertEqual(batches, expected)

  def test_get_missing_items(self):
    items = [
      isolateserver.Item('foo', 12),
      isolateserver.Item('blow', 0),
      isolateserver.Item('bizz', 1222),
      isolateserver.Item('buzz', 1223),
    ]
    missing = [
      [items[2], items[3]],
    ]

    class MockedStorageApi(isolateserver.StorageApi):
      def contains(self, _items):
        return missing
    storage = isolateserver.Storage(MockedStorageApi(), use_zip=False)

    # 'get_missing_items' is a generator, materialize its result in a list.
    result = list(storage.get_missing_items(items))
    self.assertEqual(missing, result)

  def test_async_push(self):
    for use_zip in (False, True):
      item = FakeItem('1234567')
      storage_api = self.mock_push()
      storage = isolateserver.Storage(storage_api, use_zip)
      channel = threading_utils.TaskChannel()
      storage.async_push(channel, 0, item)
      # Wait for push to finish.
      pushed_item = channel.pull()
      self.assertEqual(item, pushed_item)
      # StorageApi.push was called with correct arguments.
      self.assertEqual(
          [(item, item.zipped if use_zip else item.data)], storage_api.pushed)

  def test_async_push_generator_errors(self):
    class FakeException(Exception):
      pass

    def faulty_generator(_chunk_size):
      yield 'Hi!'
      raise FakeException('fake exception')

    for use_zip in (False, True):
      item = FakeItem('')
      self.mock(item, 'content', faulty_generator)
      storage_api = self.mock_push()
      storage = isolateserver.Storage(storage_api, use_zip)
      channel = threading_utils.TaskChannel()
      storage.async_push(channel, 0, item)
      with self.assertRaises(FakeException):
        channel.pull()
      # StorageApi's push should never complete when data can not be read.
      self.assertEqual(0, len(storage_api.pushed))

  def test_async_push_upload_errors(self):
    chunk = 'data_chunk'

    def _generator(_chunk_size):
      yield chunk

    def push_side_effect():
      raise IOError('Nope')

    # TODO(vadimsh): Retrying push when fetching data from a generator is
    # broken now (it reuses same generator instance when retrying).
    content_sources = (
        # generator(),
        lambda _chunk_size: [chunk],
    )

    for use_zip in (False, True):
      for source in content_sources:
        item = FakeItem(chunk)
        self.mock(item, 'content', source)
        storage_api = self.mock_push(push_side_effect)
        storage = isolateserver.Storage(storage_api, use_zip)
        channel = threading_utils.TaskChannel()
        storage.async_push(channel, 0, item)
        with self.assertRaises(IOError):
          channel.pull()
        # First initial attempt + all retries.
        attempts = 1 + isolateserver.WorkerPool.RETRIES
        # Single push attempt parameters.
        expected_push = (item, item.zipped if use_zip else item.data)
        # Ensure all pushes are attempted.
        self.assertEqual(
            [expected_push] * attempts, storage_api.pushed)

  def test_upload_tree(self):
    root = 'root'
    files = {
      'a': {
        's': 100,
        'h': 'hash_a',
      },
      'b': {
        's': 200,
        'h': 'hash_b',
      },
      'c': {
        's': 300,
        'h': 'hash_c',
      },
      'a_copy': {
        's': 100,
        'h': 'hash_a',
      },
    }
    files_data = dict((k, 'x' * files[k]['s']) for k in files)
    all_hashes = set(f['h'] for f in files.itervalues())
    missing_hashes = set(['hash_a', 'hash_b'])

    # Files read by mocked_file_read.
    read_calls = []
    # 'contains' calls.
    contains_calls = []
    # 'push' calls.
    push_calls = []

    def mocked_file_read(filepath, _chunk_size=0):
      self.assertEqual(root, os.path.dirname(filepath))
      filename = os.path.basename(filepath)
      self.assertIn(filename, files_data)
      read_calls.append(filename)
      return files_data[filename]
    self.mock(isolateserver, 'file_read', mocked_file_read)

    class MockedStorageApi(isolateserver.StorageApi):
      def contains(self, items):
        contains_calls.append(items)
        return [i for i in items
            if os.path.basename(i.digest) in missing_hashes]

      def push(self, item, content):
        push_calls.append((item, ''.join(content)))

    storage_api = MockedStorageApi()
    storage = isolateserver.Storage(storage_api, use_zip=False)
    storage.upload_tree(root, files)

    # Was reading only missing files.
    self.assertEqualIgnoringOrder(
        missing_hashes,
        [files[path]['h'] for path in read_calls])
    # 'contains' checked for existence of all files.
    self.assertEqualIgnoringOrder(
        all_hashes,
        [i.digest for i in sum(contains_calls, [])])
    # Pushed only missing files.
    self.assertEqualIgnoringOrder(
        missing_hashes,
        [call[0].digest for call in push_calls])
    # Pushing with correct data, size and push urls.
    for pushed_item, pushed_content in push_calls:
      filenames = [
          name for name, metadata in files.iteritems()
          if metadata['h'] == pushed_item.digest
      ]
      # If there are multiple files that map to same hash, upload_tree chooses
      # a first one.
      filename = filenames[0]
      self.assertEqual(os.path.join(root, filename), pushed_item.path)
      self.assertEqual(files_data[filename], pushed_content)


class IsolateServerStorageApiTest(TestCase):
  @staticmethod
  def mock_handshake_request(server, token='fake token', error=None):
    handshake_request = {
      'client_app_version': isolateserver.__version__,
      'fetcher': True,
      'protocol_version': isolateserver.ISOLATE_PROTOCOL_VERSION,
      'pusher': True,
    }
    handshake_response = {
      'access_token': token,
      'error': error,
      'protocol_version': isolateserver.ISOLATE_PROTOCOL_VERSION,
      'server_app_version': 'mocked server T1000',
    }
    return (
      server + '/content-gs/handshake',
      {
        'content_type': 'application/json',
        'method': 'POST',
        'data': json.dumps(handshake_request, separators=(',', ':')),
      },
      json.dumps(handshake_response),
    )

  @staticmethod
  def mock_fetch_request(server, namespace, item, data):
    return (
      server + '/content-gs/retrieve/%s/%s' % (namespace, item),
      {'retry_404': True, 'read_timeout': 60},
      data,
    )

  @staticmethod
  def mock_contains_request(server, namespace, token, request, response):
    url = server + '/content-gs/pre-upload/%s?token=%s' % (
        namespace, urllib.quote(token))
    return (
      url,
      {
        'data': json.dumps(request, separators=(',', ':')),
        'content_type': 'application/json',
        'method': 'POST',
      },
      json.dumps(response),
    )

  def test_server_capabilities_success(self):
    server = 'http://example.com'
    namespace = 'default'
    access_token = 'fake token'
    self._requests = [
      self.mock_handshake_request(server, access_token),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    caps = storage._server_capabilities
    self.assertEqual(access_token, caps['access_token'])

  def test_server_capabilities_network_failure(self):
    self.mock(isolateserver.net, 'url_open', lambda *_args, **_kwargs: None)
    with self.assertRaises(isolateserver.MappingError):
      storage = isolateserver.IsolateServer('http://example.com', 'default')
      _ = storage._server_capabilities

  def test_server_capabilities_format_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    handshake_req = self.mock_handshake_request(server)
    self._requests = [
      (handshake_req[0], handshake_req[1], 'Im a bad response'),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolateserver.MappingError):
      _ = storage._server_capabilities

  def test_server_capabilities_respects_error(self):
    server = 'http://example.com'
    namespace = 'default'
    error = 'Im sorry, Dave. Im afraid I cant do that.'
    self._requests = [
      self.mock_handshake_request(server, error=error)
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolateserver.MappingError) as context:
      _ = storage._server_capabilities
    # Server error message should be reported to user.
    self.assertIn(error, str(context.exception))

  def test_fetch_success(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = ALGO(data).hexdigest()
    self._requests = [
      self.mock_fetch_request(server, namespace, item, data),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    fetched = ''.join(storage.fetch(item))
    self.assertEqual(data, fetched)

  def test_fetch_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    item = ALGO('something').hexdigest()
    self._requests = [
      self.mock_fetch_request(server, namespace, item, None),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(IOError):
      _ = ''.join(storage.fetch(item))

  def test_push_success(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    data = ''.join(str(x) for x in xrange(1000))
    item = FakeItem(data)
    push_urls = (server + '/push_here', server + '/call_this')
    contains_request = [{'h': item.digest, 's': item.size, 'i': 0}]
    contains_response = [push_urls]
    self._requests = [
      self.mock_handshake_request(server, token),
      self.mock_contains_request(
          server, namespace, token, contains_request, contains_response),
      (
        push_urls[0],
        {
          'data': data,
          'content_type': 'application/octet-stream',
          'method': 'PUT',
        },
        ''
      ),
      (
        push_urls[1],
        {
          'data': '',
          'content_type': 'application/json',
          'method': 'POST',
        },
        ''
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing)
    storage.push(item, [data])
    self.assertTrue(item.push_state.uploaded)
    self.assertTrue(item.push_state.finalized)

  def test_push_failure_upload(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    data = ''.join(str(x) for x in xrange(1000))
    item = FakeItem(data)
    push_urls = (server + '/push_here', server + '/call_this')
    contains_request = [{'h': item.digest, 's': item.size, 'i': 0}]
    contains_response = [push_urls]
    self._requests = [
      self.mock_handshake_request(server, token),
      self.mock_contains_request(
          server, namespace, token, contains_request, contains_response),
      (
        push_urls[0],
        {
          'data': data,
          'content_type': 'application/octet-stream',
          'method': 'PUT',
        },
        None
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing)
    with self.assertRaises(IOError):
      storage.push(item, [data])
    self.assertFalse(item.push_state.uploaded)
    self.assertFalse(item.push_state.finalized)

  def test_push_failure_finalize(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    data = ''.join(str(x) for x in xrange(1000))
    item = FakeItem(data)
    push_urls = (server + '/push_here', server + '/call_this')
    contains_request = [{'h': item.digest, 's': item.size, 'i': 0}]
    contains_response = [push_urls]
    self._requests = [
      self.mock_handshake_request(server, token),
      self.mock_contains_request(
          server, namespace, token, contains_request, contains_response),
      (
        push_urls[0],
        {
          'data': data,
          'content_type': 'application/octet-stream',
          'method': 'PUT',
        },
        ''
      ),
      (
        push_urls[1],
        {
          'data': '',
          'content_type': 'application/json',
          'method': 'POST',
        },
        None
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing)
    with self.assertRaises(IOError):
      storage.push(item, [data])
    self.assertTrue(item.push_state.uploaded)
    self.assertFalse(item.push_state.finalized)

  def test_contains_success(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    files = [
      FakeItem('1', is_isolated=True),
      FakeItem('2' * 100),
      FakeItem('3' * 200),
    ]
    request = [
      {'h': files[0].digest, 's': files[0].size, 'i': 1},
      {'h': files[1].digest, 's': files[1].size, 'i': 0},
      {'h': files[2].digest, 's': files[2].size, 'i': 0},
    ]
    response = [
      None,
      ['http://example/upload_here_1', None],
      ['http://example/upload_here_2', 'http://example/call_this'],
    ]
    missing = [
      files[1],
      files[2],
    ]
    self._requests = [
      self.mock_handshake_request(server, token),
      self.mock_contains_request(server, namespace, token, request, response),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    result = storage.contains(files)
    self.assertEqual(missing, result)
    self.assertEqual(
      [x for x in response if x],
      [[i.push_state.upload_url, i.push_state.finalize_url] for i in missing])

  def test_contains_network_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    req = self.mock_contains_request(server, namespace, token, [], [])
    self._requests = [
      self.mock_handshake_request(server, token),
      (req[0], req[1], None),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolateserver.MappingError):
      storage.contains([])

  def test_contains_format_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    self._requests = [
      self.mock_handshake_request(server, token),
      self.mock_contains_request(server, namespace, token, [], [1, 2, 3])
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolateserver.MappingError):
      storage.contains([])


class IsolateServerDownloadTest(TestCase):
  tempdir = None

  def tearDown(self):
    try:
      if self.tempdir:
        shutil.rmtree(self.tempdir)
    finally:
      super(IsolateServerDownloadTest, self).tearDown()

  def test_download_two_files(self):
    # Test downloading two files.
    actual = {}
    def out(key, generator):
      actual[key] = ''.join(generator)
    self.mock(isolateserver, 'file_write', out)
    server = 'http://example.com'
    self._requests = [
      (
        server + '/content-gs/retrieve/default-gzip/sha-1',
        {'read_timeout': 60, 'retry_404': True},
        zlib.compress('Coucou'),
      ),
      (
        server + '/content-gs/retrieve/default-gzip/sha-2',
        {'read_timeout': 60, 'retry_404': True},
        zlib.compress('Bye Bye'),
      ),
    ]
    cmd = [
      'download',
      '--isolate-server', server,
      '--target', ROOT_DIR,
      '--file', 'sha-1', 'path/to/a',
      '--file', 'sha-2', 'path/to/b',
    ]
    self.assertEqual(0, isolateserver.main(cmd))
    expected = {
      os.path.join(ROOT_DIR, 'path/to/a'): 'Coucou',
      os.path.join(ROOT_DIR, 'path/to/b'): 'Bye Bye',
    }
    self.assertEqual(expected, actual)

  def test_download_isolated(self):
    # Test downloading an isolated tree.
    self.tempdir = tempfile.mkdtemp(prefix='isolateserver')
    actual = {}
    def file_write_mock(key, generator):
      actual[key] = ''.join(generator)
    self.mock(isolateserver, 'file_write', file_write_mock)
    self.mock(os, 'makedirs', lambda _: None)
    stdout = StringIO.StringIO()
    self.mock(sys, 'stdout', stdout)
    server = 'http://example.com'

    files = {
      'a/foo': 'Content',
      'b': 'More content',
      }
    isolated = {
      'command': ['Absurb', 'command'],
      'relative_cwd': 'a',
      'files': dict(
          (k, {'h': ALGO(v).hexdigest(), 's': len(v)})
          for k, v in files.iteritems()),
    }
    isolated_data = json.dumps(isolated, sort_keys=True, separators=(',',':'))
    isolated_hash = ALGO(isolated_data).hexdigest()
    requests = [(v['h'], files[k]) for k, v in isolated['files'].iteritems()]
    requests.append((isolated_hash, isolated_data))
    self._requests = [
      (
        server + '/content-gs/retrieve/default-gzip/' + h,
        {
          'read_timeout': isolateserver.DOWNLOAD_READ_TIMEOUT,
          'retry_404': True,
        },
        zlib.compress(v),
      ) for h, v in requests
    ]
    cmd = [
      'download',
      '--isolate-server', server,
      '--target', self.tempdir,
      '--isolated', isolated_hash,
    ]
    self.assertEqual(0, isolateserver.main(cmd))
    expected = dict(
        (os.path.join(self.tempdir, k), v) for k, v in files.iteritems())
    self.assertEqual(expected, actual)
    expected_stdout = (
        'To run this test please run from the directory %s:\n  Absurb command\n'
        % os.path.join(self.tempdir, 'a'))
    self.assertEqual(expected_stdout, stdout.getvalue())


class TestIsolated(unittest.TestCase):
  def test_load_isolated_empty(self):
    m = isolateserver.load_isolated('{}', None, ALGO)
    self.assertEqual({}, m)

  def test_load_isolated_good(self):
    data = {
      u'command': [u'foo', u'bar'],
      u'files': {
        u'a': {
          u'l': u'somewhere',
        },
        u'b': {
          u'm': 123,
          u'h': u'0123456789abcdef0123456789abcdef01234567',
          u's': 3,
        }
      },
      u'includes': [u'0123456789abcdef0123456789abcdef01234567'],
      u'os': 'oPhone',
      u'read_only': False,
      u'relative_cwd': u'somewhere_else'
    }
    m = isolateserver.load_isolated(json.dumps(data), None, ALGO)
    self.assertEqual(data, m)

  def test_load_isolated_bad(self):
    data = {
      u'files': {
        u'a': {
          u'l': u'somewhere',
          u'h': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
    }
    try:
      isolateserver.load_isolated(json.dumps(data), None, ALGO)
      self.fail()
    except isolateserver.ConfigError:
      pass

  def test_load_isolated_os_only(self):
    data = {
      u'os': 'HP/UX',
    }
    m = isolateserver.load_isolated(json.dumps(data), 'HP/UX', ALGO)
    self.assertEqual(data, m)

  def test_load_isolated_os_bad(self):
    data = {
      u'os': 'foo',
    }
    try:
      isolateserver.load_isolated(json.dumps(data), 'AS/400', ALGO)
      self.fail()
    except isolateserver.ConfigError:
      pass

  def test_load_isolated_path(self):
    # Automatically convert the path case.
    wrong_path_sep = u'\\' if os.path.sep == '/' else u'/'
    def gen_data(path_sep):
      return {
        u'command': [u'foo', u'bar'],
        u'files': {
          path_sep.join(('a', 'b')): {
            u'l': path_sep.join(('..', 'somewhere')),
          },
        },
        u'os': u'oPhone',
        u'relative_cwd': path_sep.join(('somewhere', 'else')),
      }

    data = gen_data(wrong_path_sep)
    actual = isolateserver.load_isolated(json.dumps(data), None, ALGO)
    expected = gen_data(os.path.sep)
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()

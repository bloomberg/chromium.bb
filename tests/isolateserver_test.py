#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# pylint: disable=W0212,W0223,W0231,W0613

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

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
import isolateserver
import test_utils
from utils import threading_utils


ALGO = hashlib.sha1

# Tests here assume ALGO is used for default namespaces, check this assumption.
assert isolateserver.get_hash_algo('default') is ALGO
assert isolateserver.get_hash_algo('default-gzip') is ALGO


class TestCase(auto_stub.TestCase):
  """Mocks out url_open() calls and sys.stdout/stderr."""
  def setUp(self):
    super(TestCase, self).setUp()
    self.mock(isolateserver.auth, 'ensure_logged_in', lambda _: None)
    self.mock(isolateserver.net, 'url_open', self._url_open)
    self.mock(isolateserver.net, 'sleep_before_retry', lambda *_: None)
    self._lock = threading.Lock()
    self._requests = []
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def tearDown(self):
    try:
      self.assertEqual([], self._requests)
      self.checkOutput('', '')
    finally:
      super(TestCase, self).tearDown()

  def checkOutput(self, expected_out, expected_err):
    try:
      self.assertEqual(expected_err, sys.stderr.getvalue())
      self.assertEqual(expected_out, sys.stdout.getvalue())
    finally:
      # Prevent double-fail.
      self.mock(sys, 'stdout', StringIO.StringIO())
      self.mock(sys, 'stderr', StringIO.StringIO())

  def _url_open(self, url, **kwargs):
    logging.warn('url_open(%s, %s)', url[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      # Ignore 'stream' argument, it's not important for these tests.
      kwargs.pop('stream', None)
      for i, n in enumerate(self._requests):
        if n[0] == url:
          _, expected_kwargs, result, headers = self._requests.pop(i)
          self.assertEqual(expected_kwargs, kwargs)
          if result is not None:
            return isolateserver.net.HttpResponse.get_fake_response(
                result, url, headers)
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
  def __init__(self, data, high_priority=False):
    super(FakeItem, self).__init__(
        ALGO(data).hexdigest(), len(data), high_priority)
    self.data = data

  def content(self):
    return [self.data]

  @property
  def zipped(self):
    return zlib.compress(self.data, self.compression_level)


class MockedStorageApi(isolateserver.StorageApi):
  def __init__(
      self, missing_hashes, push_side_effect=None, namespace='default'):
    self.missing_hashes = missing_hashes
    self.push_side_effect = push_side_effect
    self.push_calls = []
    self.contains_calls = []
    self._namespace = namespace

  @property
  def namespace(self):
    return self._namespace

  def push(self, item, push_state, content=None):
    content = ''.join(item.content() if content is None else content)
    self.push_calls.append((item, push_state, content))
    if self.push_side_effect:
      self.push_side_effect()

  def contains(self, items):
    self.contains_calls.append(items)
    missing = {}
    for item in items:
      if item.digest in self.missing_hashes:
        missing[item] = self.missing_hashes[item.digest]
    return missing


class StorageTest(TestCase):
  """Tests for Storage methods."""

  def assertEqualIgnoringOrder(self, a, b):
    """Asserts that containers |a| and |b| contain same items."""
    self.assertEqual(len(a), len(b))
    self.assertEqual(set(a), set(b))

  def get_push_state(self, storage, item):
    missing = list(storage.get_missing_items([item]))
    self.assertEqual(1, len(missing))
    self.assertEqual(item, missing[0][0])
    return missing[0][1]

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
    batches = list(isolateserver.batch_items_for_check(items))
    self.assertEqual(batches, expected)

  def test_get_missing_items(self):
    items = [
      isolateserver.Item('foo', 12),
      isolateserver.Item('blow', 0),
      isolateserver.Item('bizz', 1222),
      isolateserver.Item('buzz', 1223),
    ]
    missing = {
      items[2]: 123,
      items[3]: 456,
    }

    storage_api = MockedStorageApi(
        {item.digest: push_state for item, push_state in missing.iteritems()})
    storage = isolateserver.Storage(storage_api)

    # 'get_missing_items' is a generator yielding pairs, materialize its
    # result in a dict.
    result = dict(storage.get_missing_items(items))
    self.assertEqual(missing, result)

  def test_async_push(self):
    for use_zip in (False, True):
      item = FakeItem('1234567')
      storage_api = MockedStorageApi(
          {item.digest: 'push_state'},
          namespace='default-gzip' if use_zip else 'default')
      storage = isolateserver.Storage(storage_api)
      channel = threading_utils.TaskChannel()
      storage.async_push(channel, item, self.get_push_state(storage, item))
      # Wait for push to finish.
      pushed_item = channel.pull()
      self.assertEqual(item, pushed_item)
      # StorageApi.push was called with correct arguments.
      self.assertEqual(
          [(item, 'push_state', item.zipped if use_zip else item.data)],
          storage_api.push_calls)

  def test_async_push_generator_errors(self):
    class FakeException(Exception):
      pass

    def faulty_generator():
      yield 'Hi!'
      raise FakeException('fake exception')

    for use_zip in (False, True):
      item = FakeItem('')
      self.mock(item, 'content', faulty_generator)
      storage_api = MockedStorageApi(
          {item.digest: 'push_state'},
          namespace='default-gzip' if use_zip else 'default')
      storage = isolateserver.Storage(storage_api)
      channel = threading_utils.TaskChannel()
      storage.async_push(channel, item, self.get_push_state(storage, item))
      with self.assertRaises(FakeException):
        channel.pull()
      # StorageApi's push should never complete when data can not be read.
      self.assertEqual(0, len(storage_api.push_calls))

  def test_async_push_upload_errors(self):
    chunk = 'data_chunk'

    def _generator():
      yield chunk

    def push_side_effect():
      raise IOError('Nope')

    # TODO(vadimsh): Retrying push when fetching data from a generator is
    # broken now (it reuses same generator instance when retrying).
    content_sources = (
        # generator(),
        lambda: [chunk],
    )

    for use_zip in (False, True):
      for source in content_sources:
        item = FakeItem(chunk)
        self.mock(item, 'content', source)
        storage_api = MockedStorageApi(
            {item.digest: 'push_state'},
            push_side_effect,
            namespace='default-gzip' if use_zip else 'default')
        storage = isolateserver.Storage(storage_api)
        channel = threading_utils.TaskChannel()
        storage.async_push(channel, item, self.get_push_state(storage, item))
        with self.assertRaises(IOError):
          channel.pull()
        # First initial attempt + all retries.
        attempts = 1 + isolateserver.WorkerPool.RETRIES
        # Single push attempt call arguments.
        expected_push = (
            item, 'push_state', item.zipped if use_zip else item.data)
        # Ensure all pushes are attempted.
        self.assertEqual(
            [expected_push] * attempts, storage_api.push_calls)

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
    missing_hashes = {'hash_a': 'push a', 'hash_b': 'push b'}

    # Files read by mocked_file_read.
    read_calls = []

    def mocked_file_read(filepath, chunk_size=0, offset=0):
      self.assertEqual(root, os.path.dirname(filepath))
      filename = os.path.basename(filepath)
      self.assertIn(filename, files_data)
      read_calls.append(filename)
      return files_data[filename]
    self.mock(isolateserver, 'file_read', mocked_file_read)

    storage_api = MockedStorageApi(missing_hashes)
    storage = isolateserver.Storage(storage_api)
    def mock_get_storage(base_url, namespace):
      self.assertEqual('base_url', base_url)
      self.assertEqual('some-namespace', namespace)
      return storage
    self.mock(isolateserver, 'get_storage', mock_get_storage)

    isolateserver.upload_tree('base_url', root, files, 'some-namespace')

    # Was reading only missing files.
    self.assertEqualIgnoringOrder(
        missing_hashes,
        [files[path]['h'] for path in read_calls])
    # 'contains' checked for existence of all files.
    self.assertEqualIgnoringOrder(
        all_hashes,
        [i.digest for i in sum(storage_api.contains_calls, [])])
    # Pushed only missing files.
    self.assertEqualIgnoringOrder(
        missing_hashes,
        [call[0].digest for call in storage_api.push_calls])
    # Pushing with correct data, size and push state.
    for pushed_item, push_state, pushed_content in storage_api.push_calls:
      filenames = [
          name for name, metadata in files.iteritems()
          if metadata['h'] == pushed_item.digest
      ]
      # If there are multiple files that map to same hash, upload_tree chooses
      # a first one.
      filename = filenames[0]
      self.assertEqual(os.path.join(root, filename), pushed_item.path)
      self.assertEqual(files_data[filename], pushed_content)
      self.assertEqual(missing_hashes[pushed_item.digest], push_state)


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
      None,
    )

  @staticmethod
  def mock_fetch_request(server, namespace, item, data,
                         request_headers=None, response_headers=None):
    return (
      server + '/content-gs/retrieve/%s/%s' % (namespace, item),
      {
        'read_timeout': 60,
        'headers': request_headers,
      },
      data,
      response_headers,
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
      None,
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
      (handshake_req[0], handshake_req[1], 'Im a bad response', None),
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

  def test_fetch_offset_success(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = ALGO(data).hexdigest()
    offset = 200
    size = len(data)

    good_content_range_headers = [
      'bytes %d-%d/%d' % (offset, size - 1, size),
      'bytes %d-%d/*' % (offset, size - 1),
    ]

    for content_range_header in good_content_range_headers:
      self._requests = [
        self.mock_fetch_request(
            server, namespace, item, data[offset:],
            request_headers={'Range': 'bytes=%d-' % offset},
            response_headers={'Content-Range': content_range_header}),
      ]
      storage = isolateserver.IsolateServer(server, namespace)
      fetched = ''.join(storage.fetch(item, offset))
      self.assertEqual(data[offset:], fetched)

  def test_fetch_offset_bad_header(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = ALGO(data).hexdigest()
    offset = 200
    size = len(data)

    bad_content_range_headers = [
      # Missing header.
      None,
      '',
      # Bad format.
      'not bytes %d-%d/%d' % (offset, size - 1, size),
      'bytes %d-%d' % (offset, size - 1),
      # Bad offset.
      'bytes %d-%d/%d' % (offset - 1, size - 1, size),
      # Incomplete chunk.
      'bytes %d-%d/%d' % (offset, offset + 10, size),
    ]

    for content_range_header in bad_content_range_headers:
      self._requests = [
        self.mock_fetch_request(
            server, namespace, item, data[offset:],
            request_headers={'Range': 'bytes=%d-' % offset},
            response_headers={'Content-Range': content_range_header}),
      ]
      storage = isolateserver.IsolateServer(server, namespace)
      with self.assertRaises(IOError):
        _ = ''.join(storage.fetch(item, offset))


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
        '',
        None,
      ),
      (
        push_urls[1],
        {
          'data': '',
          'content_type': 'application/json',
          'method': 'POST',
        },
        '',
        None,
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing.keys())
    push_state = missing[item]
    storage.push(item, push_state, [data])
    self.assertTrue(push_state.uploaded)
    self.assertTrue(push_state.finalized)

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
        None,
        None,
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing.keys())
    push_state = missing[item]
    with self.assertRaises(IOError):
      storage.push(item, push_state, [data])
    self.assertFalse(push_state.uploaded)
    self.assertFalse(push_state.finalized)

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
        '',
        None,
      ),
      (
        push_urls[1],
        {
          'data': '',
          'content_type': 'application/json',
          'method': 'POST',
        },
        None,
        None,
      ),
    ]
    storage = isolateserver.IsolateServer(server, namespace)
    missing = storage.contains([item])
    self.assertEqual([item], missing.keys())
    push_state = missing[item]
    with self.assertRaises(IOError):
      storage.push(item, push_state, [data])
    self.assertTrue(push_state.uploaded)
    self.assertFalse(push_state.finalized)

  def test_contains_success(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    files = [
      FakeItem('1', high_priority=True),
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
    self.assertEqual(set(missing), set(result.keys()))
    self.assertEqual(
        [x for x in response if x],
        [[result[i].upload_url, result[i].finalize_url] for i in missing])

  def test_contains_network_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    req = self.mock_contains_request(server, namespace, token, [], [])
    self._requests = [
      self.mock_handshake_request(server, token),
      (req[0], req[1], None, None),
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


class IsolateServerStorageSmokeTest(unittest.TestCase):
  """Tests public API of Storage class using file system as a store."""

  def setUp(self):
    super(IsolateServerStorageSmokeTest, self).setUp()
    self.rootdir = tempfile.mkdtemp(prefix='isolateserver')

  def tearDown(self):
    try:
      shutil.rmtree(self.rootdir)
    finally:
      super(IsolateServerStorageSmokeTest, self).tearDown()

  def run_synchronous_push_test(self, namespace):
    storage = isolateserver.get_storage(self.rootdir, namespace)

    # Items to upload.
    items = [isolateserver.BufferItem('item %d' % i) for i in xrange(10)]

    # Storage is empty, all items are missing.
    missing = dict(storage.get_missing_items(items))
    self.assertEqual(set(items), set(missing))

    # Push, one by one.
    for item, push_state in missing.iteritems():
      storage.push(item, push_state)

    # All items are there now.
    self.assertFalse(dict(storage.get_missing_items(items)))

  def test_synchronous_push(self):
    self.run_synchronous_push_test('default')

  def test_synchronous_push_gzip(self):
    self.run_synchronous_push_test('default-gzip')

  def run_upload_items_test(self, namespace):
    storage = isolateserver.get_storage(self.rootdir, namespace)

    # Items to upload.
    items = [isolateserver.BufferItem('item %d' % i) for i in xrange(10)]

    # Do it.
    uploaded = storage.upload_items(items)
    self.assertEqual(set(items), set(uploaded))

    # All items are there now.
    self.assertFalse(dict(storage.get_missing_items(items)))

    # Now ensure upload_items skips existing items.
    more = [isolateserver.BufferItem('more item %d' % i) for i in xrange(10)]

    # Uploaded only |more|.
    uploaded = storage.upload_items(items + more)
    self.assertEqual(set(more), set(uploaded))

  def test_upload_items(self):
    self.run_upload_items_test('default')

  def test_upload_items_gzip(self):
    self.run_upload_items_test('default-gzip')

  def run_push_and_fetch_test(self, namespace):
    storage = isolateserver.get_storage(self.rootdir, namespace)

    # Upload items.
    items = [isolateserver.BufferItem('item %d' % i) for i in xrange(10)]
    uploaded = storage.upload_items(items)
    self.assertEqual(set(items), set(uploaded))

    # Fetch them all back into local memory cache.
    cache = isolateserver.MemoryCache()
    queue = isolateserver.FetchQueue(storage, cache)

    # Start fetching.
    pending = set()
    for item in items:
      pending.add(item.digest)
      queue.add(item.digest)

    # Wait for fetch to complete.
    while pending:
      fetched = queue.wait(pending)
      pending.discard(fetched)

    # Ensure fetched same data as was pushed.
    self.assertEqual(
        [i.buffer for i in items],
        [cache.read(i.digest) for i in items])

  def test_push_and_fetch(self):
    self.run_push_and_fetch_test('default')

  def test_push_and_fetch_gzip(self):
    self.run_push_and_fetch_test('default-gzip')


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
        {'read_timeout': 60, 'headers': None},
        zlib.compress('Coucou'),
        None,
      ),
      (
        server + '/content-gs/retrieve/default-gzip/sha-2',
        {'read_timeout': 60, 'headers': None},
        zlib.compress('Bye Bye'),
        None,
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
    server = 'http://example.com'

    files = {
      os.path.join('a', 'foo'): 'Content',
      'b': 'More content',
      }
    isolated = {
      'command': ['Absurb', 'command'],
      'relative_cwd': 'a',
      'files': dict(
          (k, {'h': ALGO(v).hexdigest(), 's': len(v)})
          for k, v in files.iteritems()),
      'version': isolateserver.ISOLATED_FILE_VERSION,
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
          'headers': None,
        },
        zlib.compress(v),
        None,
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
    self.checkOutput(expected_stdout, '')


class TestIsolated(auto_stub.TestCase):
  def test_load_isolated_empty(self):
    m = isolateserver.load_isolated('{}', ALGO)
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
      u'read_only': 1,
      u'relative_cwd': u'somewhere_else',
      u'version': isolateserver.ISOLATED_FILE_VERSION,
    }
    m = isolateserver.load_isolated(json.dumps(data), ALGO)
    self.assertEqual(data, m)

  def test_load_isolated_bad(self):
    data = {
      u'files': {
        u'a': {
          u'l': u'somewhere',
          u'h': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
      u'version': isolateserver.ISOLATED_FILE_VERSION,
    }
    try:
      isolateserver.load_isolated(json.dumps(data), ALGO)
      self.fail()
    except isolateserver.ConfigError:
      pass

  def test_load_isolated_os_only(self):
    # Tolerate 'os' on older version.
    data = {
      u'os': 'HP/UX',
      u'version': '1.3',
    }
    m = isolateserver.load_isolated(json.dumps(data), ALGO)
    self.assertEqual(data, m)

  def test_load_isolated_os_only_bad(self):
    data = {
      u'os': 'HP/UX',
      u'version': isolateserver.ISOLATED_FILE_VERSION,
    }
    with self.assertRaises(isolateserver.ConfigError):
      isolateserver.load_isolated(json.dumps(data), ALGO)

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
        u'relative_cwd': path_sep.join(('somewhere', 'else')),
        u'version': isolateserver.ISOLATED_FILE_VERSION,
      }

    data = gen_data(wrong_path_sep)
    actual = isolateserver.load_isolated(json.dumps(data), ALGO)
    expected = gen_data(os.path.sep)
    self.assertEqual(expected, actual)

  def test_save_isolated_good_long_size(self):
    calls = []
    self.mock(isolateserver.tools, 'write_json', lambda *x: calls.append(x))
    data = {
      u'algo': 'sha-1',
      u'files': {
        u'b': {
          u'm': 123,
          u'h': u'0123456789abcdef0123456789abcdef01234567',
          u's': 2181582786L,
        }
      },
    }
    m = isolateserver.save_isolated('foo', data)
    self.assertEqual([], m)
    self.assertEqual([('foo', data, True)], calls)


class SymlinkTest(unittest.TestCase):
  def setUp(self):
    super(SymlinkTest, self).setUp()
    self.old_cwd = os.getcwd()
    self.cwd = tempfile.mkdtemp(prefix='isolate_')
    # Everything should work even from another directory.
    os.chdir(self.cwd)

  def tearDown(self):
    try:
      os.chdir(self.old_cwd)
      shutil.rmtree(self.cwd)
    finally:
      super(SymlinkTest, self).tearDown()

  if sys.platform == 'darwin':
    def test_expand_symlinks_path_case(self):
      # Ensures that the resulting path case is fixed on case insensitive file
      # system.
      os.symlink('dest', os.path.join(self.cwd, 'link'))
      os.mkdir(os.path.join(self.cwd, 'Dest'))
      open(os.path.join(self.cwd, 'Dest', 'file.txt'), 'w').close()

      result = isolateserver.expand_symlinks(unicode(self.cwd), 'link')
      self.assertEqual((u'Dest', [u'link']), result)
      result = isolateserver.expand_symlinks(unicode(self.cwd), 'link/File.txt')
      self.assertEqual((u'Dest/file.txt', [u'link']), result)

    def test_expand_directories_and_symlinks_path_case(self):
      # Ensures that the resulting path case is fixed on case insensitive file
      # system. A superset of test_expand_symlinks_path_case.
      # Create *all* the paths with the wrong path case.
      basedir = os.path.join(self.cwd, 'baseDir')
      os.mkdir(basedir.lower())
      subdir = os.path.join(basedir, 'subDir')
      os.mkdir(subdir.lower())
      open(os.path.join(subdir, 'Foo.txt'), 'w').close()
      os.symlink('subDir', os.path.join(basedir, 'linkdir'))
      actual = isolateserver.expand_directories_and_symlinks(
          unicode(self.cwd), [u'baseDir/'], lambda _: None, True, False)
      expected = [
        u'basedir/linkdir',
        u'basedir/subdir/Foo.txt',
        u'basedir/subdir/Foo.txt',
      ]
      self.assertEqual(expected, actual)

    def test_process_input_path_case_simple(self):
      # Ensure the symlink dest is saved in the right path case.
      subdir = os.path.join(self.cwd, 'subdir')
      os.mkdir(subdir)
      linkdir = os.path.join(self.cwd, 'linkdir')
      os.symlink('subDir', linkdir)
      actual = isolateserver.process_input(
          unicode(linkdir.upper()), {}, True, ALGO)
      expected = {'l': u'subdir', 'm': 360, 't': int(os.stat(linkdir).st_mtime)}
      self.assertEqual(expected, actual)

    def test_process_input_path_case_complex(self):
      # Ensure the symlink dest is saved in the right path case. This includes 2
      # layers of symlinks.
      basedir = os.path.join(self.cwd, 'basebir')
      os.mkdir(basedir)

      linkeddir2 = os.path.join(self.cwd, 'linkeddir2')
      os.mkdir(linkeddir2)

      linkeddir1 = os.path.join(basedir, 'linkeddir1')
      os.symlink('../linkedDir2', linkeddir1)

      subsymlinkdir = os.path.join(basedir, 'symlinkdir')
      os.symlink('linkedDir1', subsymlinkdir)

      actual = isolateserver.process_input(
          unicode(subsymlinkdir.upper()), {}, True, ALGO)
      expected = {
        'l': u'linkeddir1', 'm': 360, 't': int(os.stat(subsymlinkdir).st_mtime),
      }
      self.assertEqual(expected, actual)

      actual = isolateserver.process_input(
          unicode(linkeddir1.upper()), {}, True, ALGO)
      expected = {
        'l': u'../linkeddir2', 'm': 360, 't': int(os.stat(linkeddir1).st_mtime),
      }
      self.assertEqual(expected, actual)

  if sys.platform != 'win32':
    def test_symlink_input_absolute_path(self):
      # A symlink is outside of the checkout, it should be treated as a normal
      # directory.
      # .../src
      # .../src/out -> .../tmp/foo
      # .../tmp
      # .../tmp/foo
      src = os.path.join(self.cwd, u'src')
      src_out = os.path.join(src, 'out')
      tmp = os.path.join(self.cwd, 'tmp')
      tmp_foo = os.path.join(tmp, 'foo')
      os.mkdir(src)
      os.mkdir(tmp)
      os.mkdir(tmp_foo)
      # The problem was that it's an absolute path, so it must be considered a
      # normal directory.
      os.symlink(tmp, src_out)
      open(os.path.join(tmp_foo, 'bar.txt'), 'w').close()
      actual = isolateserver.expand_symlinks(src, u'out/foo/bar.txt')
      self.assertEqual((u'out/foo/bar.txt', []), actual)


def get_storage(_isolate_server, namespace):
  class StorageFake(object):
    def __enter__(self, *_):
      return self

    def __exit__(self, *_):
      pass

    @property
    def hash_algo(self):  # pylint: disable=R0201
      return isolateserver.get_hash_algo(namespace)

    @staticmethod
    def upload_items(items):
      # Always returns the second item as not present.
      return [items[1]]
  return StorageFake()


class TestArchive(TestCase):
  @staticmethod
  def get_isolateserver_prog():
    """Returns 'isolateserver.py' or 'isolateserver.pyc'."""
    return os.path.basename(sys.modules[isolateserver.__name__].__file__)

  def test_archive_no_server(self):
    with self.assertRaises(SystemExit):
      isolateserver.main(['archive', '.'])
    prog = self.get_isolateserver_prog()
    self.checkOutput(
        '',
        'Usage: %(prog)s archive [options] <file1..fileN> or - to read '
        'from stdin\n\n'
        '%(prog)s: error: --isolate-server is required.\n' % {'prog': prog})

  def test_archive_duplicates(self):
    with self.assertRaises(SystemExit):
      isolateserver.main(
          [
            'archive', '--isolate-server', 'https://localhost:1',
            # Effective dupes.
            '.', os.getcwd(),
          ])
    prog = self.get_isolateserver_prog()
    self.checkOutput(
        '',
        'Usage: %(prog)s archive [options] <file1..fileN> or - to read '
        'from stdin\n\n'
        '%(prog)s: error: Duplicate entries found.\n' % {'prog': prog})

  def test_archive_files(self):
    old_cwd = os.getcwd()
    try:
      os.chdir(os.path.join(TEST_DIR, 'isolateserver'))
      self.mock(isolateserver, 'get_storage', get_storage)
      f = ['empty_file.txt', 'small_file.txt']
      isolateserver.main(
          ['archive', '--isolate-server', 'https://localhost:1'] + f)
      self.checkOutput(
          'da39a3ee5e6b4b0d3255bfef95601890afd80709 empty_file.txt\n'
          '0491bd1da8087ad10fcdd7c9634e308804b72158 small_file.txt\n',
          '')
    finally:
      os.chdir(old_cwd)

  def help_test_archive(self, cmd_line_prefix):
    old_cwd = os.getcwd()
    try:
      os.chdir(ROOT_DIR)
      self.mock(isolateserver, 'get_storage', get_storage)
      p = os.path.join(TEST_DIR, 'isolateserver')
      isolateserver.main(cmd_line_prefix + [p])
      # TODO(maruel): The problem here is that the test depends on the file mode
      # of the files in this directory.
      # Fix is to copy the files in a temporary directory with known file modes.
      #
      # If you modify isolateserver.ISOLATED_FILE_VERSION, you'll have to update
      # the hash below. Sorry about that.
      self.checkOutput(
          '1501166255279df1509408567340798d1cf089e7 %s\n' % p,
          '')
    finally:
      os.chdir(old_cwd)

  def test_archive_directory(self):
    self.help_test_archive(['archive', '--isolate-server',
                            'https://localhost:1'])

  def test_archive_directory_envvar(self):
    with test_utils.EnvVars({'ISOLATE_SERVER': 'https://localhost:1'}):
      self.help_test_archive(['archive'])


class OptionsTest(unittest.TestCase):
  def test_isolate_server(self):
    data = [
      (['-I', 'http://foo.com/'], 'http://foo.com'),
      (['-I', 'https://foo.com/'], 'https://foo.com'),
      (['-I', 'https://foo.com'], 'https://foo.com'),
      (['-I', 'https://foo.com/a'], 'https://foo.com/a'),
      (['-I', 'https://foo.com/a/'], 'https://foo.com/a'),
      (['-I', 'https://foo.com:8080/a/'], 'https://foo.com:8080/a'),
      (['-I', 'foo.com'], 'https://foo.com'),
      (['-I', 'foo.com:8080'], 'https://foo.com:8080'),
      (['-I', 'foo.com/'], 'https://foo.com'),
      (['-I', 'foo.com/a/'], 'https://foo.com/a'),
    ]
    for value, expected in data:
      parser = isolateserver.OptionParserIsolateServer()
      isolateserver.add_isolate_server_options(parser, False)
      options, _ = parser.parse_args(value)
      isolateserver.process_isolate_server_options(parser, options)
      self.assertEqual(expected, options.isolate_server)

  def test_indir(self):
    data = [
      (['-I', 'http://foo.com/'], ('http://foo.com', None)),
      (['--indir', ROOT_DIR], ('', ROOT_DIR)),
    ]
    for value, (expected_isolate_server, expected_indir) in data:
      parser = isolateserver.OptionParserIsolateServer()
      isolateserver.add_isolate_server_options(parser, True)
      options, _ = parser.parse_args(value)
      isolateserver.process_isolate_server_options(parser, options)
      self.assertEqual(expected_isolate_server, options.isolate_server)
      self.assertEqual(expected_indir, options.indir)


def clear_env_vars():
  for e in ('ISOLATE_DEBUG', 'ISOLATE_SERVER'):
    os.environ.pop(e, None)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  clear_env_vars()
  unittest.main()

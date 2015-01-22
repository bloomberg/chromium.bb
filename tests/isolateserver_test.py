#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# pylint: disable=W0212,W0223,W0231,W0613

import hashlib
import json
import logging
import os
import StringIO
import sys
import tempfile
import unittest
import urllib
import zlib

# net_utils adjusts sys.path.
import net_utils

import auth
import isolated_format
import isolateserver
import test_utils
from depot_tools import auto_stub
from utils import file_path
from utils import threading_utils

import isolateserver_mock


CONTENTS = {
  'empty_file.txt': '',
  'small_file.txt': 'small file\n',
  # TODO(maruel): symlinks.
}


class TestCase(net_utils.TestCase):
  """Mocks out url_open() calls and sys.stdout/stderr."""
  _tempdir = None

  def setUp(self):
    super(TestCase, self).setUp()
    self.mock(auth, 'ensure_logged_in', lambda _: None)
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())
    self.old_cwd = os.getcwd()

  def tearDown(self):
    try:
      os.chdir(self.old_cwd)
      if self._tempdir:
        file_path.rmtree(self._tempdir)
      if not self.has_failed():
        self.checkOutput('', '')
    finally:
      super(TestCase, self).tearDown()

  @property
  def tempdir(self):
    if not self._tempdir:
      self._tempdir = tempfile.mkdtemp(prefix='isolateserver')
    return self._tempdir

  def make_tree(self, contents):
    test_utils.make_tree(self.tempdir, contents)

  def checkOutput(self, expected_out, expected_err):
    try:
      self.assertEqual(expected_err, sys.stderr.getvalue())
      self.assertEqual(expected_out, sys.stdout.getvalue())
    finally:
      # Prevent double-fail.
      self.mock(sys, 'stdout', StringIO.StringIO())
      self.mock(sys, 'stderr', StringIO.StringIO())


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
      isolateserver_mock.hash_content(data), len(data), high_priority)
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
        attempts = 1 + storage.net_thread_pool.RETRIES
        # Single push attempt call arguments.
        expected_push = (
            item, 'push_state', item.zipped if use_zip else item.data)
        # Ensure all pushes are attempted.
        self.assertEqual(
            [expected_push] * attempts, storage_api.push_calls)

  def test_upload_tree(self):
    files = {
      '/a': {
        's': 100,
        'h': 'hash_a',
      },
      '/some/dir/b': {
        's': 200,
        'h': 'hash_b',
      },
      '/another/dir/c': {
        's': 300,
        'h': 'hash_c',
      },
      '/a_copy': {
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
      self.assertIn(filepath, files_data)
      read_calls.append(filepath)
      return files_data[filepath]
    self.mock(isolateserver, 'file_read', mocked_file_read)

    storage_api = MockedStorageApi(missing_hashes)
    storage = isolateserver.Storage(storage_api)
    def mock_get_storage(base_url, namespace):
      self.assertEqual('base_url', base_url)
      self.assertEqual('some-namespace', namespace)
      return storage
    self.mock(isolateserver, 'get_storage', mock_get_storage)

    isolateserver.upload_tree('base_url', files.iteritems(), 'some-namespace')

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
      self.assertEqual(filename, pushed_item.path)
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
      {'data': handshake_request},
      handshake_response,
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
    return (url, {'data': request}, response)

  def test_server_capabilities_success(self):
    server = 'http://example.com'
    namespace = 'default'
    access_token = 'fake token'
    self.expected_requests([self.mock_handshake_request(server, access_token)])
    storage = isolateserver.IsolateServer(server, namespace)
    caps = storage._server_capabilities
    self.assertEqual(access_token, caps['access_token'])

  def test_server_capabilities_network_failure(self):
    self.mock(isolateserver.net, 'url_open', lambda *_args, **_kwargs: None)
    with self.assertRaises(isolated_format.MappingError):
      storage = isolateserver.IsolateServer('http://example.com', 'default')
      _ = storage._server_capabilities

  def test_server_capabilities_format_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    handshake_req = self.mock_handshake_request(server)
    self.expected_requests(
        [(handshake_req[0], handshake_req[1], 'Im a bad response')])
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolated_format.MappingError):
      _ = storage._server_capabilities

  def test_server_capabilities_respects_error(self):
    server = 'http://example.com'
    namespace = 'default'
    error = 'Im sorry, Dave. Im afraid I cant do that.'
    self.expected_requests([self.mock_handshake_request(server, error=error)])
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolated_format.MappingError) as context:
      _ = storage._server_capabilities
    # Server error message should be reported to user.
    self.assertIn(error, str(context.exception))

  def test_fetch_success(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = isolateserver_mock.hash_content(data)
    self.expected_requests(
        [self.mock_fetch_request(server, namespace, item, data)])
    storage = isolateserver.IsolateServer(server, namespace)
    fetched = ''.join(storage.fetch(item))
    self.assertEqual(data, fetched)

  def test_fetch_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    item = isolateserver_mock.hash_content('something')
    self.expected_requests(
        [self.mock_fetch_request(server, namespace, item, None)])
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(IOError):
      _ = ''.join(storage.fetch(item))

  def test_fetch_offset_success(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = isolateserver_mock.hash_content(data)
    offset = 200
    size = len(data)

    good_content_range_headers = [
      'bytes %d-%d/%d' % (offset, size - 1, size),
      'bytes %d-%d/*' % (offset, size - 1),
    ]

    for content_range_header in good_content_range_headers:
      self.expected_requests(
          [
            self.mock_fetch_request(
                server, namespace, item, data[offset:],
                request_headers={'Range': 'bytes=%d-' % offset},
                response_headers={'Content-Range': content_range_header}),
          ])
      storage = isolateserver.IsolateServer(server, namespace)
      fetched = ''.join(storage.fetch(item, offset))
      self.assertEqual(data[offset:], fetched)

  def test_fetch_offset_bad_header(self):
    server = 'http://example.com'
    namespace = 'default'
    data = ''.join(str(x) for x in xrange(1000))
    item = isolateserver_mock.hash_content(data)
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
      self.expected_requests(
          [
            self.mock_fetch_request(
                server, namespace, item, data[offset:],
                request_headers={'Range': 'bytes=%d-' % offset},
                response_headers={'Content-Range': content_range_header}),
          ])
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
    requests = [
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
        {'content_type': 'application/json', 'data': '', 'method': 'POST'},
        '',
        None,
      ),
    ]
    self.expected_requests(requests)
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
    requests = [
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
    self.expected_requests(requests)
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
    requests = [
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
        {'content_type': 'application/json', 'data': '', 'method': 'POST'},
        None,
        None,
      ),
    ]
    self.expected_requests(requests)
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
    self.expected_requests(
        [
          self.mock_handshake_request(server, token),
          (req[0], req[1], None),
        ])
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolated_format.MappingError):
      storage.contains([])

  def test_contains_format_failure(self):
    server = 'http://example.com'
    namespace = 'default'
    token = 'fake token'
    self.expected_requests(
        [
          self.mock_handshake_request(server, token),
          self.mock_contains_request(server, namespace, token, [], [1, 2, 3]),
        ])
    storage = isolateserver.IsolateServer(server, namespace)
    with self.assertRaises(isolated_format.MappingError):
      storage.contains([])


class IsolateServerStorageSmokeTest(unittest.TestCase):
  """Tests public API of Storage class using file system as a store."""

  def setUp(self):
    super(IsolateServerStorageSmokeTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='isolateserver')
    self.server = isolateserver_mock.MockIsolateServer()

  def tearDown(self):
    try:
      self.server.close_start()
      file_path.rmtree(self.tempdir)
      self.server.close_end()
    finally:
      super(IsolateServerStorageSmokeTest, self).tearDown()

  def run_synchronous_push_test(self, namespace):
    storage = isolateserver.get_storage(self.server.url, namespace)

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
    storage = isolateserver.get_storage(self.server.url, namespace)

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
    storage = isolateserver.get_storage(self.server.url, namespace)

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

  if sys.maxsize == (2**31) - 1:
    def test_archive_multiple_huge_file(self):
      self.server.discard_content()
      # Create multiple files over 2.5gb. This test exists to stress the virtual
      # address space on 32 bits systems. Make real files since it wouldn't fit
      # memory by definition.
      # Sadly, this makes this test very slow so it's only run on 32 bits
      # platform, since it's known to work on 64 bits platforms anyway.
      #
      # It's a fairly slow test, well over 15 seconds.
      files = {}
      size = 512 * 1024 * 1024
      for i in xrange(5):
        name = '512mb_%d.%s' % (i, isolateserver.ALREADY_COMPRESSED_TYPES[0])
        p = os.path.join(self.tempdir, name)
        with open(p, 'wb') as f:
          # Write 512mb.
          h = hashlib.sha1()
          data = os.urandom(1024)
          for _ in xrange(size / 1024):
            f.write(data)
            h.update(data)
          os.chmod(p, 0600)
          files[p] = {
            'h': h.hexdigest(),
            'm': 0600,
            's': size,
          }
          if sys.platform == 'win32':
            files[p].pop('m')

      # upload_tree() is a thin wrapper around Storage.
      isolateserver.upload_tree(self.server.url, files.items(), 'testing')
      expected = {'testing': {f['h']: '<skipped>' for f in files.itervalues()}}
      self.assertEqual(expected, self.server.contents)


class IsolateServerDownloadTest(TestCase):
  def test_download_two_files(self):
    # Test downloading two files.
    actual = {}
    def out(key, generator):
      actual[key] = ''.join(generator)
    self.mock(isolateserver, 'file_write', out)
    server = 'http://example.com'
    requests = [
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
    self.expected_requests(requests)
    cmd = [
      'download',
      '--isolate-server', server,
      '--target', net_utils.ROOT_DIR,
      '--file', 'sha-1', 'path/to/a',
      '--file', 'sha-2', 'path/to/b',
    ]
    self.assertEqual(0, isolateserver.main(cmd))
    expected = {
      os.path.join(net_utils.ROOT_DIR, 'path/to/a'): 'Coucou',
      os.path.join(net_utils.ROOT_DIR, 'path/to/b'): 'Bye Bye',
    }
    self.assertEqual(expected, actual)

  def test_download_isolated(self):
    # Test downloading an isolated tree.
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
          (k, {'h': isolateserver_mock.hash_content(v), 's': len(v)})
          for k, v in files.iteritems()),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    isolated_data = json.dumps(isolated, sort_keys=True, separators=(',',':'))
    isolated_hash = isolateserver_mock.hash_content(isolated_data)
    requests = [(v['h'], files[k]) for k, v in isolated['files'].iteritems()]
    requests.append((isolated_hash, isolated_data))
    requests = [
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
    self.expected_requests(requests)
    self.assertEqual(0, isolateserver.main(cmd))
    expected = dict(
        (os.path.join(self.tempdir, k), v) for k, v in files.iteritems())
    self.assertEqual(expected, actual)
    expected_stdout = (
        'To run this test please run from the directory %s:\n  Absurb command\n'
        % os.path.join(self.tempdir, 'a'))
    self.checkOutput(expected_stdout, '')


def get_storage(_isolate_server, namespace):
  class StorageFake(object):
    def __enter__(self, *_):
      return self

    def __exit__(self, *_):
      pass

    @property
    def hash_algo(self):  # pylint: disable=R0201
      return isolated_format.get_hash_algo(namespace)

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
    self.mock(isolateserver, 'get_storage', get_storage)
    self.make_tree(CONTENTS)
    f = ['empty_file.txt', 'small_file.txt']
    os.chdir(self.tempdir)
    isolateserver.main(
        ['archive', '--isolate-server', 'https://localhost:1'] + f)
    self.checkOutput(
        'da39a3ee5e6b4b0d3255bfef95601890afd80709 empty_file.txt\n'
        '0491bd1da8087ad10fcdd7c9634e308804b72158 small_file.txt\n',
        '')

  def help_test_archive(self, cmd_line_prefix):
    self.mock(isolateserver, 'get_storage', get_storage)
    self.make_tree(CONTENTS)
    isolateserver.main(cmd_line_prefix + [self.tempdir])
    # If you modify isolated_format.ISOLATED_FILE_VERSION, you'll have to update
    # the hash below. Sorry about that but this ensures the .isolated format is
    # stable.
    isolated = {
      'algo': 'sha-1',
      'files': {},
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    for k, v in CONTENTS.iteritems():
      isolated['files'][k] = {
        'h': isolateserver_mock.hash_content(v),
        's': len(v),
      }
      if sys.platform != 'win32':
        isolated['files'][k]['m'] = 0600
    isolated_data = json.dumps(isolated, sort_keys=True, separators=(',',':'))
    isolated_hash = isolateserver_mock.hash_content(isolated_data)
    self.checkOutput(
        '%s %s\n' % (isolated_hash, self.tempdir),
        '')

  def test_archive_directory(self):
    self.help_test_archive(['archive', '-I', 'https://localhost:1'])

  def test_archive_directory_envvar(self):
    with test_utils.EnvVars({'ISOLATE_SERVER': 'https://localhost:1'}):
      self.help_test_archive(['archive'])


def clear_env_vars():
  for e in ('ISOLATE_DEBUG', 'ISOLATE_SERVER'):
    os.environ.pop(e, None)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.CRITICAL))
  clear_env_vars()
  unittest.main()

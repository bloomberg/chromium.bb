#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=W0223
# pylint: disable=W0231

import binascii
import hashlib
import json
import logging
import os
import random
import shutil
import StringIO
import sys
import tempfile
import threading
import unittest
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


class StorageTest(TestCase):
  """Tests for Storage methods."""

  @staticmethod
  def mock_push(side_effect=None):
    """Returns StorageApi subclass with mocked 'push' method."""
    class MockedStorageApi(isolateserver.StorageApi):
      def __init__(self):
        self.pushed = []
      def push(self, item, expected_size, content_generator, push_urls=None):
        self.pushed.append(
          (item, expected_size, ''.join(content_generator), push_urls))
        if side_effect:
          side_effect()
    return MockedStorageApi()

  def test_batch_files_for_check(self):
    items = {
      'foo': {'s': 12},
      'bar': {},
      'blow': {'s': 0},
      'bizz': {'s': 1222},
      'buzz': {'s': 1223},
    }
    expected = [
      [
        ('buzz', {'s': 1223}),
        ('bizz', {'s': 1222}),
        ('foo', {'s': 12}),
        ('blow', {'s': 0}),
      ],
    ]
    batches = list(isolateserver.Storage.batch_files_for_check(items))
    self.assertEqual(batches, expected)

  def test_get_missing_files(self):
    items = {
      'foo': {'s': 12},
      'bar': {},
      'blow': {'s': 0},
      'bizz': {'s': 1222},
      'buzz': {'s': 1223},
    }
    missing = {
      'bizz': {'s': 1222},
      'buzz': {'s': 1223},
    }
    fake_upload_urls = ('a', 'b')

    class MockedStorageApi(isolateserver.StorageApi):
      def contains(self, files):
        return [f + (fake_upload_urls,) for f in files if f[0] in missing]
    storage = isolateserver.Storage(MockedStorageApi(), use_zip=False)

    # 'get_missing_files' is a generator, materialize its result in a list.
    result = list(storage.get_missing_files(items))

    # Ensure it's a list of triplets.
    self.assertTrue(all(len(x) ==  3 for x in result))
    # Verify upload urls are set.
    self.assertTrue(all(x[2] == fake_upload_urls for x in result))
    # 'get_missing_files' doesn't guarantee order of its results, so convert
    # it to unordered dict and compare dicts.
    self.assertEqual(dict(x[:2] for x in result), missing)

  def test_async_push(self):
    data_to_push = '1234567'
    digest = ALGO(data_to_push).hexdigest()
    compression_level = 5
    zipped = zlib.compress(data_to_push, compression_level)
    push_urls = ('fake1', 'fake2')

    for use_zip in (False, True):
      storage_api = self.mock_push()
      storage = isolateserver.Storage(storage_api, use_zip)
      channel = threading_utils.TaskChannel()
      storage.async_push(
          channel, 0, digest, len(data_to_push), [data_to_push],
          compression_level, push_urls)
      # Wait for push to finish.
      pushed_item = channel.pull()
      self.assertEqual(digest, pushed_item)
      # StorageApi.push was called with correct arguments.
      if use_zip:
        expected_data = zipped
        expected_size = isolateserver.UNKNOWN_FILE_SIZE
      else:
        expected_data = data_to_push
        expected_size = len(data_to_push)
      self.assertEqual(
          [(digest, expected_size, expected_data, push_urls)],
          storage_api.pushed)

  def test_async_push_generator_errors(self):
    class FakeException(Exception):
      pass

    def faulty_generator():
      yield 'Hi!'
      raise FakeException('fake exception')

    for use_zip in (False, True):
      storage_api = self.mock_push()
      storage = isolateserver.Storage(storage_api, use_zip)
      channel = threading_utils.TaskChannel()
      storage.async_push(
          channel, 0, 'item', isolateserver.UNKNOWN_FILE_SIZE,
          faulty_generator(), 0, None)
      with self.assertRaises(FakeException):
        channel.pull()
      # StorageApi's push should never complete when data can not be read.
      self.assertEqual(0, len(storage_api.pushed))

  def test_async_push_upload_errors(self):
    chunk = 'data_chunk'
    compression_level = 5
    zipped = zlib.compress(chunk, compression_level)

    def _generator():
      yield chunk

    def push_side_effect():
      raise IOError('Nope')

    # TODO(vadimsh): Retrying push when fetching data from a generator is
    # broken now (it reuses same generator instance when retrying).
    content_sources = (
        # generator(),
        [chunk],
    )

    for use_zip in (False, True):
      for source in content_sources:
        storage_api = self.mock_push(push_side_effect)
        storage = isolateserver.Storage(storage_api, use_zip)
        channel = threading_utils.TaskChannel()
        storage.async_push(
            channel, 0, 'item', isolateserver.UNKNOWN_FILE_SIZE,
            source, compression_level, None)
        with self.assertRaises(IOError):
          channel.pull()
        # First initial attempt + all retries.
        attempts = 1 + isolateserver.WorkerPool.RETRIES
        # Single push attempt parameters.
        expected_push = (
            'item', isolateserver.UNKNOWN_FILE_SIZE,
            zipped if use_zip else chunk, None)
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
    }
    push_urls = {
      'a': ('upload_a', 'finalize_a'),
      'b': ('upload_b', None),
      'c': ('upload_c', None),
    }
    files_data = dict((k, k * files[k]['s']) for k in files)
    missing = set(['a', 'b'])

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
      def contains(self, files):
        contains_calls.append(files)
        return [f + (push_urls[f[0]],) for f in files if f[0] in missing]

      def push(self, item, expected_size, content_generator, push_urls=None):
        push_calls.append(
          (item, expected_size, ''.join(content_generator), push_urls))

    storage_api = MockedStorageApi()
    storage = isolateserver.Storage(storage_api, use_zip=False)
    storage.upload_tree(root, files)

    # Was reading only missing files.
    self.assertEqual(missing, set(read_calls))
    # 'contains' checked for existence of all files.
    self.assertEqual(files, dict(sum(contains_calls, [])))
    # Pushed only missing files.
    self.assertEqual(
      set(files[name]['h'] for name in missing),
      set(call[0] for call in push_calls))
    # Pushing with correct data, size and push urls.
    for push_call in push_calls:
      digest = push_call[0]
      filenames = [
          name for name, metadata in files.iteritems()
          if metadata['h'] == digest
      ]
      self.assertEqual(1, len(filenames))
      filename = filenames[0]
      data = files_data[filename]
      self.assertEqual(
          (digest, len(data), data, push_urls[filename]),
          push_call)


class IsolateServerArchiveTest(TestCase):
  def setUp(self):
    super(IsolateServerArchiveTest, self).setUp()
    self.mock(isolateserver, 'randomness', lambda: 'not_really_random')
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_present(self):
    files = [
      os.path.join(BASE_PATH, 'isolateserver', f)
      for f in ('small_file.txt', 'empty_file.txt')
    ]
    hash_encoded = ''.join(
        binascii.unhexlify(isolateserver.hash_file(f, ALGO)) for f in files)
    path = 'http://random/'
    self._requests = [
      (path + 'content/get_token', {}, 'foo bar'),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': hash_encoded, 'content_type': 'application/octet-stream'},
        '\1\1',
      ),
    ]
    result = isolateserver.main(['archive', '--isolate-server', path] + files)
    self.assertEqual(0, result)

  def test_missing(self):
    files = [
      os.path.join(BASE_PATH, 'isolateserver', f)
      for f in ('small_file.txt', 'empty_file.txt')
    ]
    hashes = [isolateserver.hash_file(f, ALGO) for f in files]
    hash_encoded = ''.join(map(binascii.unhexlify, hashes))
    compressed = [
        zlib.compress(
            open(f, 'rb').read(),
            isolateserver.get_zip_compression_level(f))
        for f in files
    ]
    path = 'http://random/'
    self._requests = [
      (path + 'content/get_token', {}, 'foo bar'),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': hash_encoded, 'content_type': 'application/octet-stream'},
        '\0\0',
      ),
      (
        path + 'content/store/default-gzip/%s?token=foo%%20bar' % hashes[0],
        {'data': compressed[0], 'content_type': 'application/octet-stream'},
        'ok',
      ),
      (
        path + 'content/store/default-gzip/%s?token=foo%%20bar' % hashes[1],
        {'data': compressed[1], 'content_type': 'application/octet-stream'},
        'ok',
      ),
    ]
    result = isolateserver.main(['archive', '--isolate-server', path] + files)
    self.assertEqual(0, result)

  def test_large(self):
    content = ''
    compressed = ''
    while (
        len(compressed) <= isolateserver.MIN_SIZE_FOR_DIRECT_BLOBSTORE):
      # The goal here is to generate a file, once compressed, is at least
      # MIN_SIZE_FOR_DIRECT_BLOBSTORE.
      content += ''.join(chr(random.randint(0, 255)) for _ in xrange(20*1024))
      compressed = zlib.compress(
          content, isolateserver.get_zip_compression_level('foo.txt'))

    s = ALGO(content).hexdigest()
    infiles = {
      'foo.txt': {
        's': len(content),
        'h': s,
      },
    }
    path = 'http://random/'
    hash_encoded = binascii.unhexlify(s)
    content_type, body = isolateserver.encode_multipart_formdata(
                [('token', 'foo bar')], [('content', s, compressed)])

    self._requests = [
      (path + 'content/get_token', {}, 'foo bar'),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': hash_encoded, 'content_type': 'application/octet-stream'},
        '\0',
      ),
      (
        path + 'content/generate_blobstore_url/default-gzip/%s' % s,
        {'data': [('token', 'foo bar')]},
        'an_url/',
      ),
      (
        'an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        'ok',
      ),
    ]

    # Setup mocks for zip_compress to return |compressed|.
    self.mock(isolateserver, 'file_read', lambda *_: None)
    self.mock(isolateserver, 'zip_compress', lambda *_: [compressed])
    result = isolateserver.upload_tree(
          base_url=path,
          indir=os.getcwd(),
          infiles=infiles,
          namespace='default-gzip')

    self.assertEqual(0, result)

  def test_upload_blobstore_simple(self):
    # A tad over 20kb so it triggers uploading to the blob store.
    content = '0123456789' * 21*1024
    s = ALGO(content).hexdigest()
    path = 'http://example.com:80/'
    data = [('token', 'a_token')]
    content_type, body = isolateserver.encode_multipart_formdata(
        data, [('content', s, content)])
    self._requests = [
      (
        path + 'content/get_token',
        {},
        'a_token',
      ),
      (
        path + 'content/generate_blobstore_url/x/' + s,
        {'data': data[:]},
        'http://example.com/an_url/',
      ),
      (
        'http://example.com/an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        'ok42',
      ),
    ]
    # |size| is currently ignored.
    result = isolateserver.IsolateServer(path, 'x').push(s, -2, [content])
    self.assertEqual('ok42', result)

  def test_upload_blobstore_retry_500(self):
    # A tad over 20kb so it triggers uploading to the blob store.
    content = '0123456789' * 21*1024
    s = ALGO(content).hexdigest()
    path = 'http://example.com:80/'
    data = [('token', 'a_token')]
    content_type, body = isolateserver.encode_multipart_formdata(
        data, [('content', s, content)])
    self._requests = [
      (
        path + 'content/get_token',
        {},
        'a_token',
      ),
      (
        path + 'content/generate_blobstore_url/x/' + s,
        {'data': data[:]},
        'http://example.com/an_url/',
      ),
      (
        'http://example.com/an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        # Let's say an HTTP 500 was returned.
        None,
      ),
      # In that case, a new url must be generated since the last one may have
      # been "consumed".
      (
        path + 'content/generate_blobstore_url/x/' + s,
        {'data': data[:]},
        'http://example.com/an_url_2/',
      ),
      (
        'http://example.com/an_url_2/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        'ok42',
      ),
    ]
    # |size| is currently ignored.
    result = isolateserver.IsolateServer(path, 'x').push(s, -2, [content])
    self.assertEqual('ok42', result)


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
        server + '/content/retrieve/default-gzip/sha-1',
        {'read_timeout': 60, 'retry_404': True},
        zlib.compress('Coucou'),
      ),
      (
        server + '/content/retrieve/default-gzip/sha-2',
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
        server + '/content/retrieve/default-gzip/' + h,
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

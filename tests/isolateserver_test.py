#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import binascii
import hashlib
import json
import logging
import os
import random
import StringIO
import sys
import threading
import unittest
import zlib

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_PATH)
sys.path.insert(0, ROOT_DIR)

import auto_stub
import isolateserver


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
            isolateserver.compression_level(f))
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
          content, isolateserver.compression_level('foo.txt'))

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

    self.mock(isolateserver, 'read_and_compress', lambda x, y: compressed)
    result = isolateserver.upload_tree(
          base_url=path,
          indir=os.getcwd(),
          infiles=infiles,
          namespace='default-gzip')

    self.assertEqual(0, result)

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
    batches = list(isolateserver.batch_files_for_check(items))
    self.assertEqual(batches, expected)

  def test_get_files_to_upload(self):
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

    def mock_check(url, items):
      self.assertEqual('fakeurl', url)
      return [item for item in items if item[0] in missing]
    self.mock(isolateserver, 'check_files_exist_on_server', mock_check)

    # 'get_files_to_upload' doesn't guarantee order of its results, so convert
    # list of pairs to unordered dict and compare dicts.
    result = dict(isolateserver.get_files_to_upload('fakeurl', items))
    self.assertEqual(result, missing)

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

  def test_zip_header_error(self):
    self.mock(
        isolateserver.net, 'url_open',
        lambda url, **_: isolateserver.net.HttpResponse.get_fake_response(
            '111', url))
    self.mock(isolateserver.time, 'sleep', lambda _x: None)

    retriever = isolateserver.get_storage_api(
        'https://fake-CAD.com/', 'namespace')
    def fetch():
      return list(retriever.fetch('foo', isolateserver.UNKNOWN_FILE_SIZE))

    with isolateserver.WorkerPool() as remote:
      # Both files will fail to be unzipped due to incorrect headers,
      # ensure that we don't accept the files (even if the size is unknown)}.
      remote.add_task(
          isolateserver.WorkerPool.MED, fetch)
      remote.add_task(isolateserver.WorkerPool.MED, fetch)
      self.assertRaises(IOError, remote.get_one_result)
      self.assertRaises(IOError, remote.get_one_result)
      # Need to use join here, since get_one_result will hang.
      self.assertEqual([], remote.join())


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
          u'm': 123,
        },
        u'b': {
          u'm': 123,
          u'h': u'0123456789abcdef0123456789abcdef01234567'
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


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()

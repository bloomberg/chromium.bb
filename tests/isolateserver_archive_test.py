#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import binascii
import random
import hashlib
import logging
import os
import StringIO
import sys
import threading
import unittest
import zlib

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_PATH)
sys.path.insert(0, ROOT_DIR)

import auto_stub
import isolateserver_archive


class IsolateServerTest(auto_stub.TestCase):
  def setUp(self):
    super(IsolateServerTest, self).setUp()
    self.mock(isolateserver_archive.run_isolated, 'url_open', self._url_open)
    self.mock(isolateserver_archive, 'randomness', lambda: 'not_really_random')
    self._lock = threading.Lock()
    self._requests = []

  def tearDown(self):
    try:
      self.assertEqual([], self._requests)
    finally:
      super(IsolateServerTest, self).tearDown()

  def _url_open(self, url, **kwargs):
    logging.warn('url_open(%s, %s)', url[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      for i, n in enumerate(self._requests):
        if n[0] == url:
          _, expected_kwargs, result = self._requests.pop(i)
          self.assertEqual(expected_kwargs, kwargs)
          return result
    self.fail('Unknown request %s' % url)

  def test_present(self):
    files = [
      os.path.join(BASE_PATH, 'isolateserver_archive', f)
      for f in ('small_file.txt', 'empty_file.txt')
    ]
    sha1encoded = ''.join(
        binascii.unhexlify(isolateserver_archive.sha1_file(f)) for f in files)
    path = 'http://random/'
    self._requests = [
      (path + 'content/get_token', {}, StringIO.StringIO('foo bar')),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': sha1encoded, 'content_type': 'application/octet-stream'},
        StringIO.StringIO('\1\1'),
      ),
    ]
    result = isolateserver_archive.main(['--remote', path] + files)
    self.assertEqual(0, result)

  def test_missing(self):
    files = [
      os.path.join(BASE_PATH, 'isolateserver_archive', f)
      for f in ('small_file.txt', 'empty_file.txt')
    ]
    sha1s = map(isolateserver_archive.sha1_file, files)
    sha1encoded = ''.join(map(binascii.unhexlify, sha1s))
    compressed = [
        zlib.compress(
            open(f, 'rb').read(),
            isolateserver_archive.compression_level(f))
        for f in files
    ]
    path = 'http://random/'
    self._requests = [
      (path + 'content/get_token', {}, StringIO.StringIO('foo bar')),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': sha1encoded, 'content_type': 'application/octet-stream'},
        StringIO.StringIO('\0\0'),
      ),
      (
        path + 'content/store/default-gzip/%s?token=foo%%20bar' % sha1s[0],
        {'data': compressed[0], 'content_type': 'application/octet-stream'},
        StringIO.StringIO('ok'),
      ),
      (
        path + 'content/store/default-gzip/%s?token=foo%%20bar' % sha1s[1],
        {'data': compressed[1], 'content_type': 'application/octet-stream'},
        StringIO.StringIO('ok'),
      ),
    ]
    result = isolateserver_archive.main(['--remote', path] + files)
    self.assertEqual(0, result)

  def test_large(self):
    content = ''
    compressed = ''
    while (
        len(compressed) <= isolateserver_archive.MIN_SIZE_FOR_DIRECT_BLOBSTORE):
      # The goal here is to generate a file, once compressed, is at least
      # MIN_SIZE_FOR_DIRECT_BLOBSTORE.
      content += ''.join(chr(random.randint(0, 255)) for _ in xrange(20*1024))
      compressed = zlib.compress(
          content, isolateserver_archive.compression_level('foo.txt'))

    s = hashlib.sha1(content).hexdigest()
    infiles = {
      'foo.txt': {
        's': len(content),
        'h': s,
      },
    }
    path = 'http://random/'
    sha1encoded = binascii.unhexlify(s)
    content_type, body = isolateserver_archive.encode_multipart_formdata(
                [('token', 'foo bar')], [('content', s, compressed)])

    self._requests = [
      (path + 'content/get_token', {}, StringIO.StringIO('foo bar')),
      (
        path + 'content/contains/default-gzip?token=foo%20bar',
        {'data': sha1encoded, 'content_type': 'application/octet-stream'},
        StringIO.StringIO('\0'),
      ),
      (
        path + 'content/generate_blobstore_url/default-gzip/%s' % s,
        {'data': [('token', 'foo bar')]},
        StringIO.StringIO('an_url/'),
      ),
      (
        'an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        StringIO.StringIO('ok'),
      ),
    ]

    self.mock(isolateserver_archive, 'read_and_compress',
              lambda x, y: compressed)
    result = isolateserver_archive.upload_sha1_tree(
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
    batches = list(isolateserver_archive.batch_files_for_check(items))
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
    self.mock(isolateserver_archive, 'check_files_exist_on_server', mock_check)

    # 'get_files_to_upload' doesn't guarantee order of its results, so convert
    # list of pairs to unordered dict and compare dicts.
    result = dict(isolateserver_archive.get_files_to_upload('fakeurl', items))
    self.assertEqual(result, missing)

  def test_upload_blobstore_simple(self):
    content = 'blob_content'
    s = hashlib.sha1(content).hexdigest()
    path = 'http://example.com:80/'
    data = [('token', 'foo bar')]
    content_type, body = isolateserver_archive.encode_multipart_formdata(
        data[:], [('content', s, 'blob_content')])
    self._requests = [
      (
        path + 'gen_url?foo#bar',
        {'data': data[:]},
        StringIO.StringIO('an_url/'),
      ),
      (
        'an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        StringIO.StringIO('ok42'),
      ),
    ]
    result = isolateserver_archive.upload_hash_content_to_blobstore(
        path + 'gen_url?foo#bar', data[:], s, content)
    self.assertEqual('ok42', result)

  def test_upload_blobstore_retry_500(self):
    content = 'blob_content'
    s = hashlib.sha1(content).hexdigest()
    path = 'http://example.com:80/'
    data = [('token', 'foo bar')]
    content_type, body = isolateserver_archive.encode_multipart_formdata(
        data[:], [('content', s, 'blob_content')])
    self._requests = [
      (
        path + 'gen_url?foo#bar',
        {'data': data[:]},
        StringIO.StringIO('an_url/'),
      ),
      (
        'an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        # Let's say an HTTP 500 was returned.
        None,
      ),
      # In that case, a new url must be generated since the last one may have
      # been "consumed".
      (
        path + 'gen_url?foo#bar',
        {'data': data[:]},
        StringIO.StringIO('an_url/'),
      ),
      (
        'an_url/',
        {'data': body, 'content_type': content_type, 'retry_50x': False},
        StringIO.StringIO('ok42'),
      ),
    ]
    result = isolateserver_archive.upload_hash_content_to_blobstore(
        path + 'gen_url?foo#bar', data[:], s, content)
    self.assertEqual('ok42', result)


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()

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

import isolateserver_archive


class IsolateServerTest(unittest.TestCase):
  def setUp(self):
    super(IsolateServerTest, self).setUp()
    self._old_url_open = isolateserver_archive.run_isolated.url_open
    isolateserver_archive.run_isolated.url_open = self._url_open
    self._lock = threading.Lock()
    self._requests = []

  def tearDown(self):
    isolateserver_archive.run_isolated.url_open = self._old_url_open
    self.assertEqual([], self._requests)
    super(IsolateServerTest, self).tearDown()

  def _url_open(self, *args, **kwargs):
    logging.warn('url_open(%s, %s)', str(args)[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      i = None
      for i, n in enumerate(self._requests):
        if n[0] == args:
          break
      expected_args, expected_kwargs, result = self._requests.pop(i)
    self.assertEqual(expected_args, args)
    self.assertEqual(expected_kwargs, kwargs)
    return result

  def test_present(self):
    files = [
      os.path.join(BASE_PATH, 'isolateserver_archive', f)
      for f in ('small_file.txt', 'empty_file.txt')
    ]
    sha1encoded = ''.join(
        binascii.unhexlify(isolateserver_archive.sha1_file(f)) for f in files)
    path = 'http://random/'
    self._requests = [
      ((path + 'content/get_token',), {}, StringIO.StringIO('foo')),
      (
        (path + 'content/contains/default-gzip?token=foo', sha1encoded),
        {'content_type': 'application/octet-stream'},
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
      ((path + 'content/get_token',), {}, StringIO.StringIO('foo')),
      (
        (path + 'content/contains/default-gzip?token=foo', sha1encoded),
        {'content_type': 'application/octet-stream'},
        StringIO.StringIO('\0\0'),
      ),
      (
        (
           path + 'content/store/default-gzip/%s?token=foo' % sha1s[0],
           compressed[0],
        ),
        {'content_type': 'application/octet-stream'},
        StringIO.StringIO('ok'),
      ),
      (
        (
          path + 'content/store/default-gzip/%s?token=foo' % sha1s[1],
          compressed[1],
        ),
        {'content_type': 'application/octet-stream'},
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
    old_randomness = isolateserver_archive.randomness
    try:
      isolateserver_archive.randomness = lambda: 'not_really_random'
      content_type, body = isolateserver_archive.encode_multipart_formdata(
                  [('token', 'foo')], [('hash_contents', s, compressed)])

      self._requests = [
        ((path + 'content/get_token',), {}, StringIO.StringIO('foo')),
        (
          (path + 'content/contains/default-gzip?token=foo', sha1encoded),
          {'content_type': 'application/octet-stream'},
          StringIO.StringIO('\0'),
        ),
        (
          (
            path + 'content/generate_blobstore_url/default-gzip/%s' % s,
            [('token', 'foo')],
          ),
          {},
          StringIO.StringIO('an_url/'),
        ),
        (
          ('an_url/', body),
          {'content_type': content_type},
          StringIO.StringIO('ok'),
        ),
      ]

      old_read_and_compress = isolateserver_archive.read_and_compress
      try:
        isolateserver_archive.read_and_compress = lambda x, y: compressed
        result = isolateserver_archive.upload_sha1_tree(
              base_url=path,
              indir=os.getcwd(),
              infiles=infiles,
              namespace='default-gzip')
      finally:
        isolateserver_archive.read_and_compress = old_read_and_compress
    finally:
      isolateserver_archive.randomness = old_randomness

    self.assertEqual(0, result)

  def test_process_items(self):
    old = isolateserver_archive.update_files_to_upload
    try:
      items = {
        'foo': {'s': 12},
        'bar': {},
        'blow': {'s': 0},
        'bizz': {'s': 1222},
        'buzz': {'s': 1223},
      }
      expected = [
        ('buzz', {'s': 1223}),
        ('bizz', {'s': 1222}),
        ('foo', {'s': 12}),
        ('blow', {'s': 0}),
      ]
      actual = []
      def process(url, items, upload_func):
        self.assertEquals('FakeUrl', url)
        self.assertEquals(self.fail, upload_func)
        actual.extend(items)

      isolateserver_archive.update_files_to_upload = process
      isolateserver_archive.process_items('FakeUrl', items, self.fail)
      self.assertEqual(expected, actual)
    finally:
      isolateserver_archive.update_files_to_upload = old


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()

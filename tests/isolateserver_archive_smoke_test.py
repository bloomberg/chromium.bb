#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import binascii
import logging
import os
import subprocess
import sys
import tempfile
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolateserver_archive

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Ensure that the testing machine has access to this server.
ISOLATE_SERVER = 'https://isolateserver.appspot.com/'

# The directory containing the test data files.
TEST_DATA_DIR = os.path.join(ROOT_DIR, 'tests', 'isolateserver_archive')


class IsolateServerArchiveSmokeTest(unittest.TestCase):
  def setUp(self):
    # The namespace must end in '-gzip' since all files are now compressed
    # before being uploaded.
    self.namespace = ('temporary' + str(long(time.time())).split('.', 1)[0]
                      + '-gzip')
    self.token = isolateserver_archive.url_open(
        ISOLATE_SERVER + '/content/get_token?from_smoke_test=1', None).read()

  def _archive_given_files(self, files):
    """Given a list of files, call isolateserver_archive.py with them. Then
    verify they are all on the server."""
    args = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolateserver_archive.py'),
        '--remote', ISOLATE_SERVER,
        '--namespace', self.namespace
    ]
    if '-v' in sys.argv:
      args.append('--verbose')
    args.extend(os.path.join(TEST_DATA_DIR, filename) for filename in files)

    self.assertEqual(0, subprocess.call(args))

    # Try to download the files from the server.
    file_hashes = [
        isolateserver_archive.sha1_file(os.path.join(TEST_DATA_DIR, f))
        for f in files
    ]
    for i in range(len(files)):
      download_url = '%scontent/retrieve/%s/%s' % (
          ISOLATE_SERVER, self.namespace, file_hashes[i])

      downloaded_file = isolateserver_archive.url_open(download_url,
                                                       retry_404=True),
      self.assertTrue(downloaded_file is not None,
                      'File %s was missing from the server' % files[i])

    # Ensure the files are listed as present on the server.
    contains_hash_url = '%scontent/contains/%s?token=%s&from_smoke_test=1' % (
        ISOLATE_SERVER, self.namespace, self.token)

    body = ''.join(binascii.unhexlify(h) for h in file_hashes)

    response = isolateserver_archive.url_open(
        contains_hash_url,
        body,
        content_type='application/octet-stream').read()

    for i in range(len(response)):
      self.assertEqual(chr(1), response[i],
                       'File %s was missing from the server' % files[i])

  def test_archive_empty_file(self):
    self._archive_given_files(['empty_file.txt'])

  def test_archive_small_file(self):
    self._archive_given_files(['small_file.txt'])

  def disabled_test_archive_huge_file(self):
    # Create a file over 2gbs.
    # TODO(maruel): Temporarily disabled until the server is fixed.
    filepath = None
    try:
      try:
        handle, filepath = tempfile.mkstemp(prefix='isolateserver')
        # Write 2.1gb.
        chunk = chr(0) + chr(57) + chr(128) + chr(255)
        chunk1mb = chunk * (1024 * 1024 / len(chunk))
        for _ in xrange(1280):
          os.write(handle, chunk1mb)
      finally:
        os.close(handle)

      self._archive_given_files([filepath])
    finally:
      if filepath:
        os.remove(filepath)


if __name__ == '__main__':
  if len(sys.argv) > 1 and sys.argv[1].startswith('http'):
    ISOLATE_SERVER = sys.argv.pop(1).rstrip('/') + '/'
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()

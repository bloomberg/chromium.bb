#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolateserver

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Ensure that the testing machine has access to this server.
ISOLATE_SERVER = 'https://isolateserver.appspot.com/'

# The directory containing the test data files.
TEST_DATA_DIR = os.path.join(ROOT_DIR, 'tests', 'isolateserver')


class IsolateServerArchiveSmokeTest(unittest.TestCase):
  def setUp(self):
    super(IsolateServerArchiveSmokeTest, self).setUp()
    # The namespace must end in '-gzip' since all files are now compressed
    # before being uploaded.
    # TODO(maruel): This should not be leaked to the client. It's a
    # transport/storage detail.
    self.namespace = ('temporary' + str(long(time.time())).split('.', 1)[0]
                      + '-gzip')
    self.rootdir = tempfile.mkdtemp(prefix='isolateserver')

  def tearDown(self):
    try:
      shutil.rmtree(self.rootdir)
    finally:
      super(IsolateServerArchiveSmokeTest, self).tearDown()

  def _run(self, args):
    """Runs isolateserver.py."""
    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolateserver.py'),
    ]
    cmd.extend(args)
    cmd.extend(
        [
          '--isolate-server', ISOLATE_SERVER,
          '--namespace', self.namespace
        ])
    if '-v' in sys.argv:
      cmd.append('--verbose')
      subprocess.check_call(cmd)
    else:
      subprocess.check_output(cmd)

  def _archive_given_files(self, files):
    """Given a list of files, call isolateserver.py with them. Then
    verify they are all on the server."""
    files = [os.path.join(TEST_DATA_DIR, filename) for filename in files]
    self._run(['archive'] + files)
    self._download_given_files(files)

  def _download_given_files(self, files):
    """Tries to download the files from the server."""
    args = ['download', '--target', self.rootdir]
    file_hashes = [isolateserver.hash_file(f, hashlib.sha1) for f in files]
    for f in file_hashes:
      args.extend(['--file', f, f])
    self._run(args)
    # Assert the files are present.
    actual = [
        isolateserver.hash_file(os.path.join(self.rootdir, f), hashlib.sha1)
        for f in os.listdir(self.rootdir)
    ]
    self.assertEqual(sorted(file_hashes), sorted(actual))

  def test_archive_empty_file(self):
    self._archive_given_files(['empty_file.txt'])

  def test_archive_small_file(self):
    self._archive_given_files(['small_file.txt'])

  def test_archive_huge_file(self):
    # Create a file over 2gbs.
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

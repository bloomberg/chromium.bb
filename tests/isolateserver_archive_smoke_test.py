#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile
import time
import unittest


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

  def _archive_given_files(self, files):
    """Given a list of files, call isolateserver_archive.py with them."""
    args = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolateserver_archive.py'),
        '--outdir', ISOLATE_SERVER,
        '--namespace', self.namespace
    ]
    if '-v' in sys.argv:
      args.append('--verbose')
    args.extend(os.path.join(TEST_DATA_DIR, filename) for filename in files)

    return subprocess.call(args)

  def test_archive_empty_file(self):
    self.assertEqual(0, self._archive_given_files(['empty_file.txt']))

  def test_archive_small_file(self):
    self.assertEqual(0, self._archive_given_files(['small_file.txt']))

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

      self.assertEqual(0, self._archive_given_files([filepath]))
    finally:
      if filepath:
        os.remove(filepath)


if __name__ == '__main__':
  unittest.main()

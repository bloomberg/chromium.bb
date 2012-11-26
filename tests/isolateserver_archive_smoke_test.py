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

# Some basic binary data stored as a byte string.
BINARY_DATA = (chr(0) + chr(57) + chr(128) + chr(255)) * 2

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
    args.extend(os.path.join(TEST_DATA_DIR, filename) for filename in files)

    process = subprocess.Popen(args)
    process.wait()

    return process.returncode

  def test_archive_empty_file(self):
    self.assertEqual(0, self._archive_given_files(['empty_file.txt']))

  def test_archive_small_file(self):
    self.assertEqual(0, self._archive_given_files(['small_file.txt']))

  def test_archive_huge_file(self):
    huge_file = None
    try:
      # Create a file over 2gbs.
      huge_file = tempfile.NamedTemporaryFile(delete=False)
      huge_file.write(BINARY_DATA * int(128 * 1024 * 1024 * 2.1))
      huge_file.close()

      self.assertEqual(0, self._archive_given_files([huge_file.name]))
    finally:
      if huge_file:
        os.remove(huge_file.name)


if __name__ == '__main__':
  unittest.main()

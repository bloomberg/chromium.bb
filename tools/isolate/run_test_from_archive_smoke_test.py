#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
VERBOSE = False


class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output, stderr, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.stderr = stderr
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s\n%s\n%s') % (
            self.cwd,
            self.output,
            self.stderr,
            ' '.join(self.cmd))


def list_files_tree(directory):
  """Returns the list of all the files in a tree."""
  actual = []
  for root, _dirs, files in os.walk(directory):
    actual.extend(os.path.join(root, f)[len(directory)+1:] for f in files)
  return sorted(actual)


def calc_sha1(filepath):
  """Calculates the SHA-1 hash for a file."""
  return hashlib.sha1(open(filepath, 'rb').read()).hexdigest()


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def write_json(filepath, data):
  with open(filepath, 'wb') as f:
    json.dump(data, f, sort_keys=True, indent=2)


class RunTestFromArchive(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp()
    self.table = os.path.join(self.tempdir, 'table')
    os.mkdir(self.table)
    self.cache = os.path.join(self.tempdir, 'cache')

    self.test = os.path.join(
        ROOT_DIR, 'data', 'run_test_from_archive', 'gtest_fake.py')
    self.test_sha1 = calc_sha1(self.test)

  def tearDown(self):
    logging.debug(self.tempdir)
    shutil.rmtree(self.tempdir)

  def _result_tree(self):
    return list_files_tree(self.tempdir)

  def _store_result(self, result_data):
    """Stores a .results file in the hash table."""
    result_text = json.dumps(result_data, sort_keys=True, indent=2)
    result_sha1 = hashlib.sha1(result_text).hexdigest()
    write_content(os.path.join(self.table, result_sha1), result_text)
    return result_sha1

  def test_result(self):
    # Store the executable in the hash table.
    shutil.copyfile(self.test, os.path.join(self.table, self.test_sha1))
    result_data = {
      'files': {
        'gtest_fake.py': {
          'sha-1': self.test_sha1,
        },
      },
      'command': ['python', 'gtest_fake.py'],
    }
    result_file = os.path.join(self.tempdir, 'foo.results')
    write_json(result_file, result_data)

    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'run_test_from_archive.py'),
      '--manifest', result_file,
      '--cache', self.cache,
      '--remote', self.table,
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    self.assertEquals(1070, len(out))
    self.assertEquals('', err)
    self.assertEquals(6, proc.returncode)

  def test_hash(self):
    # Loads the manifest from the store as a hash, then run the child.
    # Store the executable in the hash table.
    shutil.copyfile(self.test, os.path.join(self.table, self.test_sha1))
    # Store a .results file in the hash table. The file name is the content's
    # sha1, so first generate the content.
    result_data = {
      'files': {
        'gtest_fake.py': {
          'sha-1': self.test_sha1,
        },
      },
      'command': ['python', 'gtest_fake.py'],
    }
    result_sha1 = self._store_result(result_data)

    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'run_test_from_archive.py'),
      '--hash', result_sha1,
      '--cache', self.cache,
      '--remote', self.table,
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    self.assertEquals(1070, len(out))
    self.assertEquals('', err)
    self.assertEquals(6, proc.returncode)

  def test_fail_empty_manifest(self):
    result_sha1 = self._store_result({})
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'run_test_from_archive.py'),
      '--hash', result_sha1,
      '--cache', self.cache,
      '--remote', self.table,
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    self.assertEquals('', out)
    self.assertEquals('No file to map\n', err)
    self.assertEquals(1, proc.returncode)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()

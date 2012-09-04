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

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

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
    self.tempdir = tempfile.mkdtemp(prefix='run_test_from_archive_smoke_test')
    logging.debug(self.tempdir)
    # The "source" hash table.
    self.table = os.path.join(self.tempdir, 'table')
    os.mkdir(self.table)
    # The slave-side cache.
    self.cache = os.path.join(self.tempdir, 'cache')

    self.data_dir = os.path.join(ROOT_DIR, 'tests', 'run_test_from_archive')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _result_tree(self):
    return list_files_tree(self.tempdir)

  @staticmethod
  def _run(args):
    cmd = [sys.executable, os.path.join(ROOT_DIR, 'run_test_from_archive.py')]
    cmd.extend(args)
    if VERBOSE:
      cmd.extend(['-v'] * 2)
      pipe = None
    else:
      pipe = subprocess.PIPE
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=pipe, stderr=pipe, universal_newlines=True)
    out, err = proc.communicate()
    return out, err, proc.returncode

  def _store_result(self, result_data):
    """Stores a .results file in the hash table."""
    result_text = json.dumps(result_data, sort_keys=True, indent=2)
    result_sha1 = hashlib.sha1(result_text).hexdigest()
    write_content(os.path.join(self.table, result_sha1), result_text)
    return result_sha1

  def _store(self, filename):
    """Stores a test data file in the table.

    Returns its sha-1 hash.
    """
    filepath = os.path.join(self.data_dir, filename)
    h = calc_sha1(filepath)
    shutil.copyfile(filepath, os.path.join(self.table, h))
    return h

  def _generate_args(self, sha1_hash):
    """Generates the standard arguments used with sha1_hash as the hash.

    Returns a list of the required arguments.
    """
    return [
      '--hash', sha1_hash,
      '--cache', self.cache,
      '--remote', self.table,
    ]

  def test_result(self):
    # Loads an arbitrary manifest on the file system.
    manifest = os.path.join(self.data_dir, 'gtest_fake.results')
    expected = [
      'state.json',
      self._store('gtest_fake.py'),
      calc_sha1(manifest),
    ]
    args = [
      '--manifest', manifest,
      '--cache', self.cache,
      '--remote', self.table,
    ]
    out, err, returncode = self._run(args)
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals(1070, len(out), out)
    self.assertEquals(6, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_hash(self):
    # Loads the manifest from the store as a hash.
    result_sha1 = self._store('gtest_fake.results')
    expected = [
      'state.json',
      self._store('gtest_fake.py'),
      result_sha1,
    ]
    args = [
      '--hash', result_sha1,
      '--cache', self.cache,
      '--remote', self.table,
    ]
    out, err, returncode = self._run(args)
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals(1070, len(out), out)
    self.assertEquals(6, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_fail_empty_manifest(self):
    result_sha1 = self._store_result({})
    expected = [
      'state.json',
      result_sha1,
    ]
    out, err, returncode = self._run(self._generate_args(result_sha1))
    if not VERBOSE:
      self.assertEquals('', out)
      self.assertEquals('No command to run\n', err)
    self.assertEquals(1, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_includes(self):
    # Loads a manifest that includes another one.

    # References manifest1.results and gtest_fake.results. Maps file3.txt as
    # file2.txt.
    result_sha1 = self._store('check_files.results')
    expected = [
      'state.json',
      self._store('check_files.py'),
      self._store('gtest_fake.py'),
      self._store('gtest_fake.results'),
      self._store('file1.txt'),
      self._store('file3.txt'),
      # Maps file1.txt.
      self._store('manifest1.results'),
      # References manifest1.results. Maps file2.txt but it is overriden.
      self._store('manifest2.results'),
      result_sha1,
    ]
    out, err, returncode = self._run(self._generate_args(result_sha1))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals('Success\n', out)
    self.assertEquals(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_link_all_hash_instances(self):
    # Load a manifest file with the same file (same sha-1 hash), listed under
    # two different names and ensure both are created.
    result_sha1 = self._store('repeated_files.results')
    expected = [
        'state.json',
        result_sha1,
        self._store('file1.txt'),
        self._store('repeated_files.py')
    ]

    out, err, returncode = self._run(self._generate_args(result_sha1))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals('Success\n', out)
    self.assertEquals(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()

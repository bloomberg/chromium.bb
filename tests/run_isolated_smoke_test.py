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

# Imported just for the rmtree function.
import run_isolated

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


class RunSwarmStep(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp(prefix='run_isolated_smoke_test')
    logging.debug(self.tempdir)
    # The "source" hash table.
    self.table = os.path.join(self.tempdir, 'table')
    os.mkdir(self.table)
    # The slave-side cache.
    self.cache = os.path.join(self.tempdir, 'cache')

    self.data_dir = os.path.join(ROOT_DIR, 'tests', 'run_isolated')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _result_tree(self):
    return list_files_tree(self.tempdir)

  def _run(self, args):
    cmd = [sys.executable, os.path.join(ROOT_DIR, 'run_isolated.py')]
    cmd.extend(args)
    if VERBOSE:
      cmd.extend(['-v'] * 2)
      pipe = None
    else:
      pipe = subprocess.PIPE
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd,
        stdout=pipe,
        stderr=pipe,
        universal_newlines=True,
        cwd=self.tempdir)
    out, err = proc.communicate()
    return out, err, proc.returncode

  def _store_result(self, result_data):
    """Stores a .isolated file in the hash table."""
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

  def _generate_args_with_isolated(self, isolated):
    """Generates the standard arguments used with isolated as the isolated file.

    Returns a list of the required arguments.
    """
    return [
      '--isolated', isolated,
      '--cache', self.cache,
      '--remote', self.table,
    ]

  def _generate_args_with_sha1(self, sha1_hash):
    """Generates the standard arguments used with sha1_hash as the hash.

    Returns a list of the required arguments.
    """
    return [
      '--hash', sha1_hash,
      '--cache', self.cache,
      '--remote', self.table,
    ]

  def test_result(self):
    # Loads an arbitrary .isolated on the file system.
    isolated = os.path.join(self.data_dir, 'gtest_fake.isolated')
    expected = [
      'state.json',
      self._store('gtest_fake.py'),
      calc_sha1(isolated),
    ]
    out, err, returncode = self._run(
        self._generate_args_with_isolated(isolated))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals(1070, len(out), out)
    self.assertEquals(6, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_hash(self):
    # Loads the .isolated from the store as a hash.
    result_sha1 = self._store('gtest_fake.isolated')
    expected = [
      'state.json',
      self._store('gtest_fake.py'),
      result_sha1,
    ]

    out, err, returncode = self._run(self._generate_args_with_sha1(result_sha1))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals(1070, len(out), out)
    self.assertEquals(6, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_download_isolated(self):
    out_dir = None
    try:
      out_dir = tempfile.mkdtemp()

      # Store the required files.
      self._store('file1.txt')
      self._store('repeated_files.py')

      # Loads an arbitrary .isolated on the file system.
      isolated = os.path.join(self.data_dir, 'download.isolated')
      out, err, returncode = self._run(
          self._generate_args_with_isolated(isolated) +
          ['--download', out_dir])

      expected_output = ('.isolated files successfully downloaded and setup in '
                         '%s\nTo run this test please run the command %s from '
                         'the directory %s\n' %
                         (out_dir, [sys.executable, u'repeated_files.py'],
                          out_dir + os.path.sep))
      if not VERBOSE:
        self.assertEquals('', err)
        self.assertEquals(expected_output, out)
      self.assertEquals(0, returncode)

      # Ensure the correct files have been placed in the temp directory.
      self.assertTrue(os.path.exists(os.path.join(out_dir, 'file1.txt')))
      self.assertTrue(os.path.lexists(os.path.join(out_dir,
                                                   'file1_symlink.txt')))
      self.assertTrue(os.path.exists(os.path.join(out_dir, 'new_folder',
                                                  'file1.txt')))
      self.assertTrue(os.path.exists(os.path.join(out_dir,
                                                  'repeated_files.py')))
    finally:
      if out_dir:
        run_isolated.rmtree(out_dir)

  def test_fail_empty_isolated(self):
    result_sha1 = self._store_result({})
    expected = [
      'state.json',
      result_sha1,
    ]
    out, err, returncode = self._run(self._generate_args_with_sha1(result_sha1))
    if not VERBOSE:
      self.assertEquals('', out)
      self.assertEquals('No command to run\n', err)
    self.assertEquals(1, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_includes(self):
    # Loads an .isolated that includes another one.

    # References manifest1.isolated and gtest_fake.isolated. Maps file3.txt as
    # file2.txt.
    result_sha1 = self._store('check_files.isolated')
    expected = [
      'state.json',
      self._store('check_files.py'),
      self._store('gtest_fake.py'),
      self._store('gtest_fake.isolated'),
      self._store('file1.txt'),
      self._store('file3.txt'),
      # Maps file1.txt.
      self._store('manifest1.isolated'),
      # References manifest1.isolated. Maps file2.txt but it is overriden.
      self._store('manifest2.isolated'),
      result_sha1,
    ]
    out, err, returncode = self._run(self._generate_args_with_sha1(result_sha1))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals('Success\n', out)
    self.assertEquals(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_link_all_hash_instances(self):
    # Load an isolated file with the same file (same sha-1 hash), listed under
    # two different names and ensure both are created.
    result_sha1 = self._store('repeated_files.isolated')
    expected = [
        'state.json',
        result_sha1,
        self._store('file1.txt'),
        self._store('repeated_files.py')
    ]

    out, err, returncode = self._run(self._generate_args_with_sha1(result_sha1))
    if not VERBOSE:
      self.assertEquals('', err)
      self.assertEquals('Success\n', out)
    self.assertEquals(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEquals(sorted(expected), actual)

  def test_delete_invalid_cache_entry(self):
    isolated_file = os.path.join(self.data_dir, 'file_with_size.isolated')
    file1_sha1 = self._store('file1.txt')

    # Run the test once to generate the cache.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)

    # Modify one of the files in the cache to be invalid.
    cached_file_path = os.path.join(self.cache, file1_sha1)
    with open(cached_file_path, 'w') as f:
      f.write('invalid size')
    logging.info('Modified %s', cached_file_path)
    # Ensure that the cache has an invalid file.
    self.assertNotEqual(
        os.stat(os.path.join(self.data_dir, 'file1.txt')).st_size,
        os.stat(cached_file_path).st_size)

    # Rerun the test and make sure the cache contains the right file afterwards.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)

    self.assertEqual(os.stat(os.path.join(self.data_dir, 'file1.txt')).st_size,
                     os.stat(cached_file_path).st_size)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()

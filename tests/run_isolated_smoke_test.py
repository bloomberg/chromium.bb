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
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolateserver
import run_isolated

VERBOSE = False

ALGO = hashlib.sha1


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


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def write_json(filepath, data):
  with open(filepath, 'wb') as f:
    json.dump(data, f, sort_keys=True, indent=2)


class RunSwarmStep(unittest.TestCase):
  def setUp(self):
    self.tempdir = run_isolated.make_temp_dir(
        'run_isolated_smoke_test', ROOT_DIR)
    logging.debug(self.tempdir)
    # run_isolated.zip executable package.
    self.run_isolated_zip = os.path.join(self.tempdir, 'run_isolated.zip')
    run_isolated.get_as_zip_package().zip_into_file(
        self.run_isolated_zip, compress=False)
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
    cmd = [sys.executable, self.run_isolated_zip]
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
    result_hash = ALGO(result_text).hexdigest()
    write_content(os.path.join(self.table, result_hash), result_text)
    return result_hash

  def _store(self, filename):
    """Stores a test data file in the table.

    Returns its sha-1 hash.
    """
    filepath = os.path.join(self.data_dir, filename)
    h = isolateserver.hash_file(filepath, ALGO)
    shutil.copyfile(filepath, os.path.join(self.table, h))
    return h

  def _generate_args_with_isolated(self, isolated):
    """Generates the standard arguments used with isolated as the isolated file.

    Returns a list of the required arguments.
    """
    return [
      '--isolated', isolated,
      '--cache', self.cache,
      '--isolate-server', self.table,
      '--namespace', 'default',
    ]

  def _generate_args_with_hash(self, hash_value):
    """Generates the standard arguments used with |hash_value| as the hash.

    Returns a list of the required arguments.
    """
    return [
      '--hash', hash_value,
      '--cache', self.cache,
      '--isolate-server', self.table,
      '--namespace', 'default',
    ]

  def test_result(self):
    # Loads an arbitrary .isolated on the file system.
    isolated = os.path.join(self.data_dir, 'repeated_files.isolated')
    expected = [
      'state.json',
      self._store('file1.txt'),
      self._store('file1_copy.txt'),
      self._store('repeated_files.py'),
      isolateserver.hash_file(isolated, ALGO),
    ]
    out, err, returncode = self._run(
        self._generate_args_with_isolated(isolated))
    if not VERBOSE:
      self.assertEqual('Success\n', out, (out, err))
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(set(expected)), actual)

  def test_hash(self):
    # Loads the .isolated from the store as a hash.
    result_hash = self._store('repeated_files.isolated')
    expected = [
      'state.json',
      self._store('file1.txt'),
      self._store('file1_copy.txt'),
      self._store('repeated_files.py'),
      result_hash,
    ]

    out, err, returncode = self._run(self._generate_args_with_hash(result_hash))
    if not VERBOSE:
      self.assertEqual('', err)
      self.assertEqual('Success\n', out, out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(set(expected)), actual)

  def test_fail_empty_isolated(self):
    result_hash = self._store_result({})
    expected = [
      'state.json',
      result_hash,
    ]
    out, err, returncode = self._run(self._generate_args_with_hash(result_hash))
    if not VERBOSE:
      self.assertEqual('', out)
      self.assertEqual('No command to run\n', err)
    self.assertEqual(1, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(expected), actual)

  def test_includes(self):
    # Loads an .isolated that includes another one.

    # References manifest2.isolated and repeated_files.isolated. Maps file3.txt
    # as file2.txt.
    result_hash = self._store('check_files.isolated')
    expected = [
      'state.json',
      self._store('check_files.py'),
      self._store('file1.txt'),
      self._store('file3.txt'),
      # Maps file1.txt.
      self._store('manifest1.isolated'),
      # References manifest1.isolated. Maps file2.txt but it is overriden.
      self._store('manifest2.isolated'),
      result_hash,
      self._store('repeated_files.py'),
      self._store('repeated_files.isolated'),
    ]
    out, err, returncode = self._run(self._generate_args_with_hash(result_hash))
    if not VERBOSE:
      self.assertEqual('', err)
      self.assertEqual('Success\n', out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(expected), actual)

  def test_link_all_hash_instances(self):
    # Load an isolated file with the same file (same sha-1 hash), listed under
    # two different names and ensure both are created.
    result_hash = self._store('repeated_files.isolated')
    expected = [
        'state.json',
        result_hash,
        self._store('file1.txt'),
        self._store('repeated_files.py')
    ]

    out, err, returncode = self._run(self._generate_args_with_hash(result_hash))
    if not VERBOSE:
      self.assertEqual('', err)
      self.assertEqual('Success\n', out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(expected), actual)

  def test_delete_invalid_cache_entry(self):
    isolated_file = os.path.join(self.data_dir, 'file_with_size.isolated')
    file1_hash = self._store('file1.txt')

    # Run the test once to generate the cache.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)

    # Modify one of the files in the cache to be invalid.
    cached_file_path = os.path.join(self.cache, file1_hash)
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

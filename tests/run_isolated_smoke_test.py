#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

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


def read_content(filepath):
  with open(filepath, 'rb') as f:
    return f.read()


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def tree_modes(root):
  """Returns the dict of files in a directory with their filemode.

  Includes |root| as '.'.
  """
  out = {}
  offset = len(root.rstrip('/\\')) + 1
  out['.'] = oct(os.stat(root).st_mode)
  for dirpath, dirnames, filenames in os.walk(root):
    for filename in filenames:
      p = os.path.join(dirpath, filename)
      out[p[offset:]] = oct(os.stat(p).st_mode)
    for dirname in dirnames:
      p = os.path.join(dirpath, dirname)
      out[p[offset:]] = oct(os.stat(p).st_mode)
  return out


class RunIsolatedTest(unittest.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
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
    run_isolated.rmtree(self.tempdir)
    super(RunIsolatedTest, self).tearDown()

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
    # Need to know the hash before writting the file.
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
      '--indir', self.table,
      '--namespace', 'default',
    ]

  def _generate_args_with_hash(self, hash_value):
    """Generates the standard arguments used with |hash_value| as the hash.

    Returns a list of the required arguments.
    """
    return [
      '--hash', hash_value,
      '--cache', self.cache,
      '--indir', self.table,
      '--namespace', 'default',
    ]

  def assertTreeModes(self, root, expected):
    """Compares the file modes of everything in |root| with |expected|.

    Arguments:
      root: directory to list its tree.
      expected: dict(relpath: (linux_mode, mac_mode, win_mode)) where each mode
                is the expected file mode on this OS. For practical purposes,
                linux is "anything but OSX or Windows". The modes should be
                ints.
    """
    actual = tree_modes(root)
    if sys.platform == 'win32':
      index = 2
    elif sys.platform == 'darwin':
      index = 1
    else:
      index = 0
    expected_mangled = dict((k, oct(v[index])) for k, v in expected.iteritems())
    self.assertEqual(expected_mangled, actual)


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
      self.assertIn('No command to run\n', err)
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

  def test_delete_quite_corrupted_cache_entry(self):
    # Test that an entry with an invalid file size properly gets removed and
    # fetched again. This test case also check for file modes.
    isolated_file = os.path.join(self.data_dir, 'file_with_size.isolated')
    isolated_hash = isolateserver.hash_file(isolated_file, ALGO)
    file1_hash = self._store('file1.txt')
    # Note that <tempdir>/table/<file1_hash> has 640 mode.

    # Run the test once to generate the cache.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)
    expected = {
      '.': (040775, 040755, 040777),
      'state.json': (0100664, 0100644, 0100666),
      # The reason for 0100666 on Windows is that the file node had to be
      # modified to delete the hardlinked node. The read only bit is reset on
      # load.
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)

    # Modify one of the files in the cache to be invalid.
    cached_file_path = os.path.join(self.cache, file1_hash)
    previous_mode = os.stat(cached_file_path).st_mode
    os.chmod(cached_file_path, 0600)
    old_content = read_content(cached_file_path)
    write_content(cached_file_path, old_content + ' but now invalid size')
    os.chmod(cached_file_path, previous_mode)
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
    expected = {
      '.': (040700, 040700, 040777),
      'state.json': (0100600, 0100600, 0100666),
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)

    self.assertEqual(os.stat(os.path.join(self.data_dir, 'file1.txt')).st_size,
                     os.stat(cached_file_path).st_size)
    self.assertEqual(old_content, read_content(cached_file_path))

  def test_delete_slightly_corrupted_cache_entry(self):
    # Test that an entry with an invalid file size properly gets removed and
    # fetched again. This test case also check for file modes.
    isolated_file = os.path.join(self.data_dir, 'file_with_size.isolated')
    isolated_hash = isolateserver.hash_file(isolated_file, ALGO)
    file1_hash = self._store('file1.txt')
    # Note that <tempdir>/table/<file1_hash> has 640 mode.

    # Run the test once to generate the cache.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)
    expected = {
      '.': (040775, 040755, 040777),
      'state.json': (0100664, 0100644, 0100666),
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)

    # Modify one of the files in the cache to be invalid.
    cached_file_path = os.path.join(self.cache, file1_hash)
    previous_mode = os.stat(cached_file_path).st_mode
    os.chmod(cached_file_path, 0600)
    old_content = read_content(cached_file_path)
    write_content(cached_file_path, old_content[1:] + 'b')
    os.chmod(cached_file_path, previous_mode)
    logging.info('Modified %s', cached_file_path)
    self.assertEqual(
        os.stat(os.path.join(self.data_dir, 'file1.txt')).st_size,
        os.stat(cached_file_path).st_size)

    # Rerun the test and make sure the cache contains the right file afterwards.
    out, err, returncode = self._run(self._generate_args_with_isolated(
        isolated_file))
    if VERBOSE:
      print out
      print err
    self.assertEqual(0, returncode)
    expected = {
      '.': (040700, 040700, 040777),
      'state.json': (0100600, 0100600, 0100666),
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)

    self.assertEqual(os.stat(os.path.join(self.data_dir, 'file1.txt')).st_size,
                     os.stat(cached_file_path).st_size)
    # TODO(maruel): This corruption is NOT detected.
    # This needs to be fixed.
    self.assertNotEqual(old_content, read_content(cached_file_path))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()

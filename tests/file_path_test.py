#!/usr/bin/env python
# coding=utf-8
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import tempfile
import unittest
import shutil
import sys

BASE_DIR = unicode(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

FILE_PATH = unicode(os.path.abspath(__file__))

from utils import file_path


class FilePath(unittest.TestCase):
  def test_native_case_end_with_os_path_sep(self):
    # Make sure the trailing os.path.sep is kept.
    path = file_path.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEqual(file_path.get_native_path_case(path), path)

  def test_native_case_end_with_dot_os_path_sep(self):
    path = file_path.get_native_path_case(ROOT_DIR + os.path.sep)
    self.assertEqual(
        file_path.get_native_path_case(path + '.' + os.path.sep),
        path)

  def test_native_case_non_existing(self):
    # Make sure it doesn't throw on non-existing files.
    non_existing = 'trace_input_test_this_file_should_not_exist'
    path = os.path.expanduser('~/' + non_existing)
    self.assertFalse(os.path.exists(path))
    path = file_path.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEqual(file_path.get_native_path_case(path), path)

  if sys.platform in ('darwin', 'win32'):
    def test_native_case_not_sensitive(self):
      # The home directory is almost guaranteed to have mixed upper/lower case
      # letters on both Windows and OSX.
      # This test also ensures that the output is independent on the input
      # string case.
      path = os.path.expanduser(u'~')
      self.assertTrue(os.path.isdir(path))
      path = path.replace('/', os.path.sep)
      if sys.platform == 'win32':
        # Make sure the drive letter is upper case for consistency.
        path = path[0].upper() + path[1:]
      # This test assumes the variable is in the native path case on disk, this
      # should be the case. Verify this assumption:
      self.assertEqual(path, file_path.get_native_path_case(path))
      self.assertEqual(
          file_path.get_native_path_case(path.lower()),
          file_path.get_native_path_case(path.upper()))

    def test_native_case_not_sensitive_non_existent(self):
      # This test also ensures that the output is independent on the input
      # string case.
      non_existing = os.path.join(
          'trace_input_test_this_dir_should_not_exist', 'really not', '')
      path = os.path.expanduser(os.path.join(u'~', non_existing))
      path = path.replace('/', os.path.sep)
      self.assertFalse(os.path.exists(path))
      lower = file_path.get_native_path_case(path.lower())
      upper = file_path.get_native_path_case(path.upper())
      # Make sure non-existing element is not modified:
      self.assertTrue(lower.endswith(non_existing.lower()))
      self.assertTrue(upper.endswith(non_existing.upper()))
      self.assertEqual(lower[:-len(non_existing)], upper[:-len(non_existing)])

  if sys.platform != 'win32':
    def test_symlink(self):
      # This test will fail if the checkout is in a symlink.
      actual = file_path.split_at_symlink(None, ROOT_DIR)
      expected = (ROOT_DIR, None, None)
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), None, None)
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), 'files2', '')
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '')
      self.assertEqual(expected, actual)
      actual = file_path.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2', 'bar'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '/bar')
      self.assertEqual(expected, actual)

    def test_native_case_symlink_right_case(self):
      actual = file_path.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs'))
      self.assertEqual('trace_inputs', os.path.basename(actual))

      # Make sure the symlink is not resolved.
      actual = file_path.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      self.assertEqual('files2', os.path.basename(actual))

  if sys.platform == 'darwin':
    def test_native_case_symlink_wrong_case(self):
      base_dir = file_path.get_native_path_case(BASE_DIR)
      trace_inputs_dir = os.path.join(base_dir, 'trace_inputs')
      actual = file_path.get_native_path_case(trace_inputs_dir)
      self.assertEqual(trace_inputs_dir, actual)

      # Make sure the symlink is not resolved.
      data = os.path.join(trace_inputs_dir, 'Files2')
      actual = file_path.get_native_path_case(data)
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2'), actual)

      data = os.path.join(trace_inputs_dir, 'Files2', '')
      actual = file_path.get_native_path_case(data)
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2', ''), actual)

      data = os.path.join(trace_inputs_dir, 'Files2', 'Child1.py')
      actual = file_path.get_native_path_case(data)
      # TODO(maruel): Should be child1.py.
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2', 'Child1.py'), actual)

  if sys.platform == 'win32':
    def test_native_case_alternate_datastream(self):
      # Create the file manually, since tempfile doesn't support ADS.
      tempdir = unicode(tempfile.mkdtemp(prefix='trace_inputs'))
      try:
        tempdir = file_path.get_native_path_case(tempdir)
        basename = 'foo.txt'
        filename = basename + ':Zone.Identifier'
        filepath = os.path.join(tempdir, filename)
        open(filepath, 'w').close()
        self.assertEqual(filepath, file_path.get_native_path_case(filepath))
        data_suffix = ':$DATA'
        self.assertEqual(
            filepath + data_suffix,
            file_path.get_native_path_case(filepath + data_suffix))

        open(filepath + '$DATA', 'w').close()
        self.assertEqual(
            filepath + data_suffix,
            file_path.get_native_path_case(filepath + data_suffix))
        # Ensure the ADS weren't created as separate file. You love NTFS, don't
        # you?
        self.assertEqual([basename], os.listdir(tempdir))
      finally:
        shutil.rmtree(tempdir)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()

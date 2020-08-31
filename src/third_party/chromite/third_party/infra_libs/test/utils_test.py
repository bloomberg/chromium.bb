# -*- encoding: utf-8 -*-
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import infra_libs


DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')


class _UtilTestException(Exception):
  """Exception used inside tests."""


class TemporaryDirectoryTest(unittest.TestCase):
  def test_tempdir_no_error(self):
    with infra_libs.temporary_directory() as tempdir:
      self.assertTrue(os.path.isdir(tempdir))
      # This should work.
      with open(os.path.join(tempdir, 'test_tempdir_no_error.txt'), 'w') as f:
        f.write('nonsensical content')
    # And everything should have been cleaned up afterward
    self.assertFalse(os.path.isdir(tempdir))

  def test_tempdir_with_exception(self):
    with self.assertRaises(_UtilTestException):
      with infra_libs.temporary_directory() as tempdir:
        self.assertTrue(os.path.isdir(tempdir))
        # Create a non-empty file to check that tempdir deletion works.
        with open(os.path.join(tempdir, 'test_tempdir_no_error.txt'), 'w') as f:
          f.write('nonsensical content')
        raise _UtilTestException()

    # And everything should have been cleaned up afterward
    self.assertFalse(os.path.isdir(tempdir))

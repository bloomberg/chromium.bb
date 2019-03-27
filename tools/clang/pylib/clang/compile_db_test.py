#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Tests for compile_db."""

import sys
import unittest

import compile_db


# Input compile DB.
_TEST_COMPILE_DB = [
    # Verifies that gomacc.exe is removed.
    {
        'command': r'C:\gomacc.exe C:\clang-cl.exe /blah',
    },
    # Verifies a goma path containing a space.
    {
        'command': r'"C:\Program Files\gomacc.exe" C:\clang-cl.exe /blah',
    },
    # Includes a string define.
    {
        'command': r'clang-cl.exe /blah "-DCR_CLANG_REVISION=\"346388-1\""',
    },
    # Includes a string define with a space in it.
    {
        'command': r'clang-cl.exe /blah -D"MY_DEFINE=\"MY VALUE\""',
    },
]

# Expected compile DB after processing for windows.
_EXPECTED_COMPILE_DB = [
    {
        'command': r'C:\clang-cl.exe --driver-mode=cl /blah',
    },
    {
        'command': r'C:\clang-cl.exe --driver-mode=cl /blah',
    },
    {
        'command': r'clang-cl.exe --driver-mode=cl /blah '
                   r'"-DCR_CLANG_REVISION=\"346388-1\""',
    },
    {
        'command': r'clang-cl.exe --driver-mode=cl /blah '
                   r'-D"MY_DEFINE=\"MY VALUE\""',
    },
]


class CompileDbTest(unittest.TestCase):

  def setUp(self):
    self.maxDiff = None

  def testProcessNotOnWindows(self):
    sys.platform = 'linux2'
    processed_compile_db = compile_db.ProcessCompileDatabaseIfNeeded(
        _TEST_COMPILE_DB)

    # Assert no changes were made.
    self.assertItemsEqual(processed_compile_db, _TEST_COMPILE_DB)

  def testProcessForWindows(self):
    sys.platform = 'win32'
    processed_compile_db = compile_db.ProcessCompileDatabaseIfNeeded(
        _TEST_COMPILE_DB)

    # Check each entry individually to improve readability of the output.
    for actual, expected in zip(processed_compile_db, _EXPECTED_COMPILE_DB):
      self.assertDictEqual(actual, expected)


if __name__ == '__main__':
  unittest.main()

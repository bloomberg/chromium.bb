#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for trychange.py."""

import os
import unittest

# Local imports
import trychange


class TryChangeTestsBase(unittest.TestCase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    pass

  def tearDown(self):
    pass

  def compareMembers(self, object, members):
    """If you add a member, be sure to add the relevant test!"""
    # Skip over members starting with '_' since they are usually not meant to
    # be for public use.
    actual_members = [x for x in sorted(dir(object))
                      if not x.startswith('_')]
    expected_members = sorted(members)
    if actual_members != expected_members:
      diff = ([i for i in actual_members if i not in expected_members] +
              [i for i in expected_members if i not in actual_members])
      print diff
    self.assertEqual(actual_members, expected_members)


class TryChangeUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'EscapeDot', 'ExecuteTryServerScript', 'GIT', 'GetSourceRoot', 'GuessVCS',
      'HELP_STRING', 'InvalidScript', 'NoTryServerAccess', 'PathDifference',
      'RunCommand', 'SCM', 'SCRIPT_PATH', 'SVN', 'TryChange', 'USAGE',
      'datetime', 'gcl', 'gclient', 'getpass', 'logging', 'optparse', 'os',
      'shutil', 'sys', 'tempfile', 'traceback', 'urllib',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange, members)


class SVNUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'ProcessOptions', 'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.SVN(None), members)


class TryChangeUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'GetEmail', 'GetPatchName', 'ProcessOptions', 'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.GIT(None), members)


if __name__ == '__main__':
  unittest.main()

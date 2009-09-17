#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for trychange.py."""

import unittest

# Local imports
import super_mox
import trychange
from super_mox import mox


class TryChangeTestsBase(super_mox.SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  pass


class TryChangeUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'EscapeDot', 'GIT', 'GetSourceRoot',
      'GetTryServerSettings', 'GuessVCS',
      'HELP_STRING', 'InvalidScript', 'NoTryServerAccess', 'PathDifference',
      'RunCommand', 'SCM', 'SVN', 'TryChange', 'USAGE',
      'datetime', 'gcl', 'gclient', 'gclient_scm', 'getpass', 'logging',
      'optparse', 'os', 'shutil', 'socket', 'sys', 'tempfile', 'traceback',
      'upload', 'urllib',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange, members)


class SVNUnittest(TryChangeTestsBase):
  """trychange.SVN tests."""
  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'ProcessOptions', 'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.SVN(None), members)


class GITUnittest(TryChangeTestsBase):
  """trychange.GIT tests."""
  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'GetEmail', 'GetPatchName', 'ProcessOptions', 'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.GIT(None), members)


if __name__ == '__main__':
  unittest.main()

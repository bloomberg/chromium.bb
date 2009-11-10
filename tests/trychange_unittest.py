#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for trychange.py."""

import optparse
import unittest

# Local imports
import gcl
import super_mox
import trychange
import upload
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
      'datetime', 'gcl', 'gclient_scm', 'getpass', 'logging',
      'optparse', 'os', 'presubmit_support', 'shutil', 'socket', 'subprocess',
      'sys', 'tempfile', 'upload', 'urllib',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange, members)


class SVNUnittest(TryChangeTestsBase):
  """trychange.SVN tests."""
  def setUp(self):
    self.fake_root = '/fake_root'
    self.expected_files = ['foo.txt', 'bar.txt']
    change_info = gcl.ChangeInfo('test_change', 0, 0, 'desc',
                                 [('M', f) for f in self.expected_files],
                                 self.fake_root)
    self.svn = trychange.SVN(None)
    self.svn.change_info = change_info
    super_mox.SuperMoxTestBase.setUp(self)

  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'GetFileNames', 'GetLocalRoot', 'ProcessOptions',
      'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.SVN(None), members)

  def testGetFileNames(self):
    self.mox.ReplayAll()
    self.assertEqual(self.svn.GetFileNames(), self.expected_files)

  def testGetLocalRoot(self):
    self.mox.ReplayAll()
    self.assertEqual(self.svn.GetLocalRoot(), self.fake_root)


class GITUnittest(TryChangeTestsBase):
  """trychange.GIT tests."""
  def setUp(self):
    self.fake_root = gcl.os.path.join(gcl.os.path.dirname(__file__),
                                      'fake_root')
    self.expected_files = ['foo.txt', 'bar.txt']
    options = optparse.Values()
    options.files = self.expected_files
    self.git = trychange.GIT(options)
    super_mox.SuperMoxTestBase.setUp(self)

  def testMembersChanged(self):
    members = [
      'GenerateDiff', 'GetEmail', 'GetFileNames', 'GetLocalRoot',
      'GetPatchName', 'ProcessOptions', 'options'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.GIT(None), members)

  def testGetFileNames(self):
    self.mox.ReplayAll()
    self.assertEqual(self.git.GetFileNames(), self.expected_files)

  def testGetLocalRoot(self):
    self.mox.StubOutWithMock(upload, 'RunShell')
    upload.RunShell(['git', 'rev-parse', '--show-cdup']).AndReturn(
        self.fake_root)
    self.mox.ReplayAll()
    self.assertEqual(self.git.GetLocalRoot(), self.fake_root)


if __name__ == '__main__':
  unittest.main()

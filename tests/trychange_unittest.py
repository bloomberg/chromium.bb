#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for trychange.py."""

# pylint: disable=E1103,W0403

# Fixes include path.
from super_mox import SuperMoxTestBase

import trychange


class TryChangeTestsBase(SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(trychange.gclient_utils, 'CheckCall')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'Capture')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GenerateDiff')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GetCheckoutRoot')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GetEmail')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GetPatchName')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GetUpstreamBranch')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'DiffItem')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'GenerateDiff')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'GetCheckoutRoot')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'GetEmail')
    self.fake_root = self.Dir()
    self.expected_files = ['foo.txt', 'bar.txt']
    self.options = trychange.optparse.Values()
    self.options.files = self.expected_files
    self.options.diff = None
    self.options.name = None
    self.options.email = None
    self.options.exclude = []


class TryChangeUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'EPILOG', 'Escape', 'GIT', 'GuessVCS', 'GetMungedDiff', 'HELP_STRING',
      'InvalidScript', 'NoTryServerAccess', 'PrintSuccess', 'SCM', 'SVN',
      'TryChange', 'USAGE',
      'breakpad', 'datetime', 'errno', 'fix_encoding', 'gcl', 'gclient_utils',
      'getpass',
      'json', 'logging', 'optparse', 'os', 'posixpath', 're', 'scm', 'shutil',
      'subprocess2', 'sys', 'tempfile', 'urllib',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange, members)


class SVNUnittest(TryChangeTestsBase):
  """trychange.SVN tests."""
  def testMembersChanged(self):
    members = [
      'AutomagicalSettings', 'GetCodeReviewSetting', 'ReadRootFile',
      'GenerateDiff', 'GetFileNames',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.SVN, members)

  def testBasic(self):
    trychange.scm.SVN.GetCheckoutRoot(self.fake_root).AndReturn(self.fake_root)
    trychange.scm.SVN.GenerateDiff(['foo.txt', 'bar.txt'],
                                   self.fake_root,
                                   full_move=True,
                                   revision=None).AndReturn('A diff')
    trychange.scm.SVN.GetEmail(self.fake_root).AndReturn('georges@example.com')
    self.mox.ReplayAll()
    svn = trychange.SVN(self.options, self.fake_root)
    self.assertEqual(svn.GetFileNames(), self.expected_files)
    self.assertEqual(svn.checkout_root, self.fake_root)
    self.assertEqual(svn.GenerateDiff(), 'A diff')


class GITUnittest(TryChangeTestsBase):
  """trychange.GIT tests."""
  def testMembersChanged(self):
    members = [
      'AutomagicalSettings', 'GetCodeReviewSetting', 'ReadRootFile',
      'GenerateDiff', 'GetFileNames',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.GIT, members)

  def testBasic(self):
    trychange.scm.GIT.GetCheckoutRoot(self.fake_root).AndReturn(self.fake_root)
    trychange.scm.GIT.GetUpstreamBranch(self.fake_root).AndReturn('somewhere')
    trychange.scm.GIT.GenerateDiff(self.fake_root,
                                   full_move=True,
                                   files=['foo.txt', 'bar.txt'],
                                   branch='somewhere').AndReturn('A diff')
    trychange.scm.GIT.GetPatchName(self.fake_root).AndReturn('bleh-1233')
    trychange.scm.GIT.GetEmail(self.fake_root).AndReturn('georges@example.com')
    self.mox.ReplayAll()
    git = trychange.GIT(self.options, self.fake_root)
    self.assertEqual(git.GetFileNames(), self.expected_files)
    self.assertEqual(git.checkout_root, self.fake_root)
    self.assertEqual(git.GenerateDiff(), 'A diff')


if __name__ == '__main__':
  import unittest
  unittest.main()

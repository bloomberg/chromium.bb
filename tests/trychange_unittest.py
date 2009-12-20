#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for trychange.py."""

import optparse

# Local imports
import trychange
from super_mox import mox, SuperMoxTestBase


class TryChangeTestsBase(SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(trychange.gclient_utils, 'CheckCall')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'Capture')
    self.mox.StubOutWithMock(trychange.scm.GIT, 'GetEmail')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'DiffItem')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'GetCheckoutRoot')
    self.mox.StubOutWithMock(trychange.scm.SVN, 'GetEmail')
    self.fake_root = self.Dir()
    self.expected_files = ['foo.txt', 'bar.txt']
    self.options = optparse.Values()
    self.options.files = self.expected_files
    self.options.diff = None
    self.options.name = None
    self.options.email = None


class TryChangeUnittest(TryChangeTestsBase):
  """General trychange.py tests."""
  def testMembersChanged(self):
    members = [
      'EscapeDot', 'GIT', 'GetTryServerSettings', 'GuessVCS',
      'HELP_STRING', 'InvalidScript', 'NoTryServerAccess',
      'SCM', 'SVN', 'TryChange', 'USAGE', 'breakpad',
      'datetime', 'gcl', 'gclient_utils', 'getpass', 'logging',
      'optparse', 'os', 'presubmit_support', 'scm', 'shutil', 'socket',
      'subprocess', 'sys', 'tempfile', 'urllib',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange, members)


class SVNUnittest(TryChangeTestsBase):
  """trychange.SVN tests."""
  def testMembersChanged(self):
    members = [
      'GetFileNames', 'GetLocalRoot',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.SVN, members)

  def testBasic(self):
    trychange.os.getcwd().AndReturn(self.fake_root)
    trychange.scm.SVN.GetCheckoutRoot(self.fake_root).AndReturn(self.fake_root)
    trychange.os.getcwd().AndReturn('pro')
    trychange.os.chdir(self.fake_root)
    trychange.scm.SVN.DiffItem(self.expected_files[0]).AndReturn('bleh')
    trychange.scm.SVN.DiffItem(self.expected_files[1]).AndReturn('blew')
    trychange.os.chdir('pro')
    trychange.scm.SVN.GetEmail(self.fake_root).AndReturn('georges@example.com')
    self.mox.ReplayAll()
    svn = trychange.SVN(self.options)
    self.assertEqual(svn.GetFileNames(), self.expected_files)
    self.assertEqual(svn.GetLocalRoot(), self.fake_root)


class GITUnittest(TryChangeTestsBase):
  """trychange.GIT tests."""
  def testMembersChanged(self):
    members = [
      'GetFileNames', 'GetLocalRoot',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(trychange.GIT, members)

  def testBasic(self):
    trychange.gclient_utils.CheckCall(
        ['git', 'rev-parse', '--show-cdup']).AndReturn(self.fake_root)
    trychange.os.path.abspath(self.fake_root).AndReturn(self.fake_root)
    trychange.gclient_utils.CheckCall(
        ['git', 'cl', 'upstream']).AndReturn('random branch')
    trychange.gclient_utils.CheckCall(
        ['git', 'diff-tree', '-p', '--no-prefix', 'random branch', 'HEAD']
        ).AndReturn('This is a dummy diff\n+3\n-4\n')
    trychange.gclient_utils.CheckCall(
        ['git', 'symbolic-ref', 'HEAD']).AndReturn('refs/heads/another branch')
    trychange.scm.GIT.GetEmail('.').AndReturn('georges@example.com')
    self.mox.ReplayAll()
    git = trychange.GIT(self.options)
    self.assertEqual(git.GetFileNames(), self.expected_files)
    self.assertEqual(git.GetLocalRoot(), self.fake_root)


if __name__ == '__main__':
  import unittest
  unittest.main()

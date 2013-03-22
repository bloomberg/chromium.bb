#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_as_stable.py."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.scripts import cros_mark_as_stable


# pylint: disable=W0212,R0904
class NonClassTests(cros_test_lib.MoxTestCase):
  def setUp(self):
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommandCaptureOutput')
    self._branch = 'test_branch'
    self._target_manifest_branch = 'cros/master'

  def testPushChange(self):
    git_log = 'Marking test_one as stable\nMarking test_two as stable\n'
    fake_description = 'Marking set of ebuilds as stable\n\n%s' % git_log
    self.mox.StubOutWithMock(cros_mark_as_stable, '_DoWeHaveLocalCommits')
    self.mox.StubOutWithMock(cros_mark_as_stable.GitBranch, 'CreateBranch')
    self.mox.StubOutWithMock(cros_mark_as_stable.GitBranch, 'Exists')
    self.mox.StubOutWithMock(git, 'PushWithRetry')
    self.mox.StubOutWithMock(git, 'GetTrackingBranch')
    self.mox.StubOutWithMock(git, 'SyncPushBranch')
    self.mox.StubOutWithMock(git, 'CreatePushBranch')
    self.mox.StubOutWithMock(git, 'RunGit')

    cros_mark_as_stable._DoWeHaveLocalCommits(
        self._branch, self._target_manifest_branch, '.').AndReturn(True)
    git.GetTrackingBranch('.', for_push=True).AndReturn(
        ['gerrit', 'refs/remotes/gerrit/master'])
    git.SyncPushBranch('.', 'gerrit', 'refs/remotes/gerrit/master')
    cros_mark_as_stable._DoWeHaveLocalCommits(
        self._branch, 'refs/remotes/gerrit/master', '.').AndReturn(True)
    result = cros_build_lib.CommandResult(output=git_log)
    cros_build_lib.RunCommandCaptureOutput(
        ['git', 'log', '--format=format:%s%n%n%b',
         'refs/remotes/gerrit/master..%s' % self._branch],
        cwd='.').AndReturn(result)
    git.CreatePushBranch('merge_branch', '.')
    git.RunGit('.', ['merge', '--squash', self._branch])
    git.RunGit('.', ['commit', '-m', fake_description])
    git.RunGit('.', ['config', 'push.default', 'tracking'])
    git.PushWithRetry('merge_branch', '.', dryrun=False)
    self.mox.ReplayAll()
    cros_mark_as_stable.PushChange(self._branch, self._target_manifest_branch,
                                   False, '.')
    self.mox.VerifyAll()


class CleanStalePackagesTest(cros_build_lib_unittest.RunCommandTestCase):

  def setUp(self):
    self.PatchObject(osutils, 'FindMissingBinaries', return_value=[])

  def testNormalClean(self):
    """Clean up boards/packages with normal success"""
    cros_mark_as_stable.CleanStalePackages(('board1', 'board2'), ['cow', 'car'])

  def testNothingToUnmerge(self):
    """Clean up packages that don't exist (portage will exit 1)"""
    self.rc.AddCmdResult(partial_mock.In('emerge'), returncode=1)
    cros_mark_as_stable.CleanStalePackages((), ['no/pkg'])

  def testUnmergeError(self):
    """Make sure random exit errors are not ignored"""
    self.rc.AddCmdResult(partial_mock.In('emerge'), returncode=123)
    with parallel_unittest.ParallelMock():
      self.assertRaises(cros_build_lib.RunCommandError,
                        cros_mark_as_stable.CleanStalePackages,
                        (), ['no/pkg'])


class GitBranchTest(cros_test_lib.MoxTestCase):

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommandCaptureOutput')
    self._branch = self.mox.CreateMock(cros_mark_as_stable.GitBranch)
    self._branch_name = 'test_branch'
    self._branch.branch_name = self._branch_name
    self._target_manifest_branch = 'cros/test'
    self._branch.tracking_branch = self._target_manifest_branch
    self._branch.cwd = '.'

  def testCheckoutCreate(self):
    # Test init with no previous branch existing.
    self._branch.Exists(self._branch_name).AndReturn(False)
    cros_build_lib.RunCommandCaptureOutput(['repo', 'start', self._branch_name,
                                            '.'], print_cmd=False, cwd='.')
    self.mox.ReplayAll()
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.mox.VerifyAll()

  def testCheckoutNoCreate(self):
    # Test init with previous branch existing.
    self._branch.Exists(self._branch_name).AndReturn(True)
    cros_build_lib.RunCommandCaptureOutput(['git', 'checkout', '-f',
                                            self._branch_name], print_cmd=False,
                                           cwd='.')
    self.mox.ReplayAll()
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.mox.VerifyAll()

  def testExists(self):
    branch = cros_mark_as_stable.GitBranch(self._branch_name,
                                           self._target_manifest_branch, '.')
    # Test if branch exists that is created
    result = cros_build_lib.CommandResult(output=self._branch_name + '\n')
    cros_build_lib.RunCommandCaptureOutput(['git', 'branch'], print_cmd=False,
                                           cwd='.').AndReturn(result)
    self.mox.ReplayAll()
    self.assertTrue(branch.Exists())
    self.mox.VerifyAll()


if __name__ == '__main__':
  cros_test_lib.main()

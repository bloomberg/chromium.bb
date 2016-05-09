# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_as_stable.py."""

from __future__ import print_function

import mock

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.scripts import cros_mark_as_stable


class RunGitMock(partial_mock.PartialCmdMock):
  """Partial mock for git.RunMock."""
  TARGET = 'chromite.lib.git'
  ATTRS = ('RunGit',)
  DEFAULT_ATTR = 'RunGit'

  def RunGit(self, _git_repo, cmd, _retry=True, **kwargs):
    return self._results['RunGit'].LookupResult(
        (cmd,), hook_args=(cmd,), hook_kwargs=kwargs)


class NonClassTests(cros_test_lib.MockTestCase):
  """Test the flow for pushing a change."""

  def setUp(self):
    self._branch = 'test_branch'
    self._target_manifest_branch = 'cros/master'

  def _TestPushChange(self, bad_cls):
    side_effect = Exception('unittest says this should not be called')

    git_log = 'Marking test_one as stable\nMarking test_two as stable\n'
    fake_description = 'Marking set of ebuilds as stable\n\n%s' % git_log
    self.PatchObject(git, 'DoesCommitExistInRepo', return_value=True)
    self.PatchObject(cros_mark_as_stable, '_DoWeHaveLocalCommits',
                     return_value=True)
    self.PatchObject(cros_mark_as_stable.GitBranch, 'CreateBranch',
                     side_effect=side_effect)
    self.PatchObject(cros_mark_as_stable.GitBranch, 'Exists',
                     side_effect=side_effect)

    push_mock = self.PatchObject(git, 'PushWithRetry')
    self.PatchObject(
        git, 'GetTrackingBranch',
        return_value=git.RemoteRef('gerrit', 'refs/remotes/gerrit/master'))
    sync_mock = self.PatchObject(git, 'SyncPushBranch')
    create_mock = self.PatchObject(git, 'CreatePushBranch')
    git_mock = self.StartPatcher(RunGitMock())

    git_mock.AddCmdResult(['checkout', self._branch])

    cmd = ['log', '--format=short', '--perl-regexp', '--author',
           '^(?!chrome-bot)', 'refs/remotes/gerrit/master..%s' % self._branch]

    if bad_cls:
      push_mock.side_effect = side_effect
      create_mock.side_effect = side_effect
      git_mock.AddCmdResult(cmd, output='Found bad stuff')
    else:
      git_mock.AddCmdResult(cmd, output='\n')
      cmd = ['log', '--format=format:%s%n%n%b',
             'refs/remotes/gerrit/master..%s' % self._branch]
      git_mock.AddCmdResult(cmd, output=git_log)
      git_mock.AddCmdResult(['merge', '--squash', self._branch])
      git_mock.AddCmdResult(['commit', '-m', fake_description])
      git_mock.AddCmdResult(['config', 'push.default', 'tracking'])

    cros_mark_as_stable.PushChange(self._branch, self._target_manifest_branch,
                                   False, '.')
    sync_mock.assert_called_with('.', 'gerrit', 'refs/remotes/gerrit/master')
    if not bad_cls:
      push_mock.assert_called_with('merge_branch', '.', dryrun=False,
                                   staging_branch=None)
      create_mock.assert_called_with('merge_branch', '.',
                                     remote_push_branch=mock.ANY)

  def testPushChange(self):
    """Verify pushing changes works."""
    self._TestPushChange(bad_cls=False)

  def testPushChangeBadCls(self):
    """Verify we do not push bad CLs."""
    self.assertRaises(AssertionError, self._TestPushChange, bad_cls=True)


class CleanStalePackagesTest(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for cros_mark_as_stable.CleanStalePackages."""

  def setUp(self):
    self.PatchObject(osutils, 'FindMissingBinaries', return_value=[])

  def testNormalClean(self):
    """Clean up boards/packages with normal success"""
    cros_mark_as_stable.CleanStalePackages('.', ('board1', 'board2'),
                                           ['cow', 'car'])

  def testNothingToUnmerge(self):
    """Clean up packages that don't exist (portage will exit 1)"""
    self.rc.AddCmdResult(partial_mock.In('emerge'), returncode=1)
    cros_mark_as_stable.CleanStalePackages('.', (), ['no/pkg'])

  def testUnmergeError(self):
    """Make sure random exit errors are not ignored"""
    self.rc.AddCmdResult(partial_mock.In('emerge'), returncode=123)
    with parallel_unittest.ParallelMock():
      self.assertRaises(cros_build_lib.RunCommandError,
                        cros_mark_as_stable.CleanStalePackages,
                        '.', (), ['no/pkg'])


class GitBranchTest(cros_test_lib.MockTestCase):
  """Tests for cros_mark_as_stable.GitBranch."""

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.rc_mock = self.PatchObject(cros_build_lib, 'RunCommand')

    self._branch_name = 'test_branch'
    self._target_manifest_branch = 'cros/test'
    self._branch = cros_mark_as_stable.GitBranch(
        branch_name=self._branch_name,
        tracking_branch=self._target_manifest_branch,
        cwd='.')

  def testCheckoutCreate(self):
    """Test init with no previous branch existing."""
    self.PatchObject(self._branch, 'Exists', return_value=False)
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.rc_mock.assert_call(mock.call(
        ['repo', 'start', self._branch_name, '.'],
        print_cmd=False, cwd='.', capture_output=True))

  def testCheckoutNoCreate(self):
    """Test init with previous branch existing."""
    self.PatchObject(self._branch, 'Exists', return_value=True)
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.rc_mock.assert_call(mock.call(
        ['git', 'checkout', '-f', self._branch_name],
        print_cmd=False, cwd='.', capture_output=True))

  def testExists(self):
    """Test if branch exists that is created."""
    result = cros_build_lib.CommandResult(output=self._branch_name + '\n')
    self.PatchObject(git, 'RunGit', return_value=result)
    self.assertTrue(self._branch.Exists())

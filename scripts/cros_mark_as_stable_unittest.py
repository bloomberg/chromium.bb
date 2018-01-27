# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_as_stable.py."""

from __future__ import print_function

import mock
import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import portage_util
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


class EbuildMock(object):
  """Mock portage_util.Ebuild."""

  def __init__(self, path, new_package=True):
    self.path = path
    self.package = '%s_package' % path
    self.cros_workon_vars = 'cros_workon_vars'
    self._new_package = new_package

  # pylint: disable=unused-argument
  def RevWorkOnEBuild(self, srcroot, manifest, redirect_file=None):
    if self._new_package:
      return ('%s_new_package' % self.path,
              '%s_new_ebuild' % self.path,
              '%s_old_ebuild' % self.path)


# pylint: disable=protected-access
class MarkAsStableParallelTest(cros_test_lib.MockTestCase):
  """Test mark_as_stable in parallel."""

  def setUp(self):
    self._overlay = 'test-overlay'
    self._manifest = 'manifest'
    self._parser = cros_mark_as_stable.GetParser()

    self._manager = parallel.Manager()
    self.ebuild_paths_to_add = self._manager.list()
    self.ebuild_paths_to_remove = self._manager.list()
    self.revved_packages = self._manager.list()
    self.new_package_atoms = self._manager.list()
    self.messages = self._manager.list()
    self._manager.__enter__()

    self.PatchObject(git, 'GetTrackingBranchViaManifest')

  def tearDown(self):
    # Mimic exiting a 'with' statement.
    self._manager.__exit__(None, None, None)

  def testWorkOnEbuildWithNewPackage(self):
    """Test _WorkOnEbuild with new packages."""
    ebuild = EbuildMock('ebuild')
    options = self._parser.parse_args(['commit'])

    cros_mark_as_stable._WorkOnEbuild(
        self._overlay, ebuild, self._manifest, options,
        self.ebuild_paths_to_add, self.ebuild_paths_to_remove,
        self.messages, self.revved_packages, self.new_package_atoms)
    self.assertItemsEqual(self.ebuild_paths_to_add, ['ebuild_new_ebuild'])
    self.assertItemsEqual(self.ebuild_paths_to_remove, ['ebuild_old_ebuild'])
    self.assertItemsEqual(self.messages,
                          [cros_mark_as_stable._GIT_COMMIT_MESSAGE %
                           'ebuild_package'])
    self.assertItemsEqual(self.revved_packages, ['ebuild_package'])
    self.assertItemsEqual(self.new_package_atoms, ['=ebuild_new_package'])

  def testWorkOnEbuildWithoutNewPackage(self):
    """Test _WorkOnEbuild without new packages."""
    ebuild = EbuildMock('ebuild', new_package=False)
    options = self._parser.parse_args(['commit'])

    cros_mark_as_stable._WorkOnEbuild(
        self._overlay, ebuild, self._manifest, options,
        self.ebuild_paths_to_add, self.ebuild_paths_to_remove,
        self.messages, self.revved_packages, self.new_package_atoms)
    self.assertEqual(len(self.ebuild_paths_to_add), 0)
    self.assertEqual(len(self.ebuild_paths_to_remove), 0)
    self.assertEqual(len(self.messages), 0)
    self.assertEqual(len(self.revved_packages), 0)
    self.assertEqual(len(self.new_package_atoms), 0)

  def testWorkOnOverlayWithPushCmd(self):
    """Test _WorkOnOverlay with Push command."""
    ebuild_1 = EbuildMock('ebuild_1')
    ebuild_2 = EbuildMock('ebuild_2')
    ebuilds = [ebuild_1, ebuild_2]
    overlay_ebuilds = {self._overlay: ebuilds}
    options = self._parser.parse_args(['push'])

    push_change_mock = self.PatchObject(cros_mark_as_stable, 'PushChange')
    self.PatchObject(os.path, 'isdir', return_value=True)

    cros_mark_as_stable._WorkOnOverlay(
        self._overlay, overlay_ebuilds, self._manifest, options,
        self.ebuild_paths_to_add, self.ebuild_paths_to_remove,
        self.messages, self.revved_packages, self.new_package_atoms)

    push_change_mock.assert_called_once_with(
        constants.STABLE_EBUILD_BRANCH, mock.ANY,
        options.dryrun, cwd=self._overlay,
        staging_branch=options.staging_branch)

  def testWorkOnOverlayWithCommitCmd(self):
    """Test _WorkOnOverlay with Commit command."""
    ebuild_1 = EbuildMock('ebuild_1')
    ebuild_2 = EbuildMock('ebuild_2')
    ebuilds = [ebuild_1, ebuild_2]
    overlay_ebuilds = {self._overlay: ebuilds}
    options = self._parser.parse_args(['commit'])

    self.PatchObject(git, 'GetGitRepoRevision')
    self.PatchObject(git, 'RunGit')
    self.PatchObject(cros_mark_as_stable.GitBranch, 'CreateBranch')
    self.PatchObject(cros_mark_as_stable.GitBranch, 'Exists', return_value=True)
    self.PatchObject(portage_util.EBuild, 'CommitChange')
    self.PatchObject(os.path, 'isdir', return_value=True)

    cros_mark_as_stable._WorkOnOverlay(
        self._overlay, overlay_ebuilds, self._manifest, options,
        self.ebuild_paths_to_add, self.ebuild_paths_to_remove,
        self.messages, self.revved_packages, self.new_package_atoms)

    self.assertItemsEqual(
        self.ebuild_paths_to_add,
        ['ebuild_1_new_ebuild', 'ebuild_2_new_ebuild'])
    self.assertItemsEqual(
        self.ebuild_paths_to_remove,
        ['ebuild_1_old_ebuild', 'ebuild_2_old_ebuild'])
    self.assertItemsEqual(
        self.messages,
        [cros_mark_as_stable._GIT_COMMIT_MESSAGE % 'ebuild_1_package',
         cros_mark_as_stable._GIT_COMMIT_MESSAGE % 'ebuild_2_package'])
    self.assertItemsEqual(
        self.revved_packages,
        ['ebuild_1_package', 'ebuild_2_package'])
    self.assertItemsEqual(
        self.new_package_atoms,
        ['=ebuild_1_new_package', '=ebuild_2_new_package'])

  def testWorkOnOverlayNoEbuildsWithCommitCmd(self):
    """Test _WorkOnOverlay without ebuilds with Commit command."""
    overlay_ebuilds = {self._overlay: []}
    options = self._parser.parse_args(['commit'])

    self.PatchObject(git, 'GetGitRepoRevision')
    self.PatchObject(git, 'RunGit')
    self.PatchObject(cros_mark_as_stable.GitBranch, 'CreateBranch')
    self.PatchObject(cros_mark_as_stable.GitBranch, 'Exists', return_value=True)
    self.PatchObject(os.path, 'isdir', return_value=True)

    cros_mark_as_stable._WorkOnOverlay(
        self._overlay, overlay_ebuilds, self._manifest, options,
        self.ebuild_paths_to_add, self.ebuild_paths_to_remove,
        self.messages, self.revved_packages, self.new_package_atoms)
    self.assertItemsEqual(self.ebuild_paths_to_add, [])
    self.assertItemsEqual(self.ebuild_paths_to_remove, [])
    self.assertItemsEqual(self.messages, [])
    self.assertItemsEqual(self.revved_packages, [])
    self.assertItemsEqual(self.new_package_atoms, [])


class MainTests(cros_build_lib_unittest.RunCommandTestCase,
                cros_test_lib.MockTempDirTestCase):
  """Tests for cros_mark_as_stable.main()."""

  def setUp(self):
    self.PatchObject(git.ManifestCheckout, 'Cached', return_value='manifest')
    self.PatchObject(cros_mark_as_stable, 'CleanStalePackages')
    self.mock_work_on_overlay = self.PatchObject(
        cros_mark_as_stable, '_WorkOnOverlay')

  def testMainWithProvidedOverlays(self):
    """Test Main with overlays provided in options."""
    overlays = []
    for i in range(0, 3):
      overlay = os.path.join(self.tempdir, 'overlay_%s' % i)
      osutils.SafeMakedirs(overlay)
      overlays.append(overlay)

    self.PatchObject(portage_util, 'GetOverlayEBuilds', return_value=['ebuild'])

    cros_mark_as_stable.main(
        ['commit', '--all', '--overlays', ':'.join(overlays)])
    self.assertEqual(self.mock_work_on_overlay.call_count, 3)


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
      with self.assertRaises(cros_build_lib.RunCommandError):
        cros_mark_as_stable.CleanStalePackages('.', (), ['no/pkg'])


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
    self.rc_mock.assert_called_with(
        ['repo', 'start', self._branch_name, '.'],
        print_cmd=False, cwd='.', capture_output=True)

  def testCheckoutNoCreate(self):
    """Test init with previous branch existing."""
    self.PatchObject(self._branch, 'Exists', return_value=True)
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.rc_mock.assert_called_with(
        ['git', 'checkout', '-f', self._branch_name],
        print_cmd=False, cwd='.', capture_output=True)

  def testExists(self):
    """Test if branch exists that is created."""
    result = cros_build_lib.CommandResult(output=self._branch_name + '\n')
    self.PatchObject(git, 'RunGit', return_value=result)
    self.assertTrue(self._branch.Exists())

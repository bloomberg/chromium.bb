# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.scripts.cbuildbot_launch."""

from __future__ import print_function

import mock
import os
import time

from chromite.cbuildbot import repository
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cbuildbot_launch

EXPECTED_MANIFEST_URL = 'https://chrome-internal-review.googlesource.com/chromeos/manifest-internal'  # pylint: disable=line-too-long


# It's reasonable for unittests to look at internals.
# pylint: disable=protected-access


class FakeException(Exception):
  """Test exception to raise during tests."""


class CbuildbotLaunchTest(cros_test_lib.MockTestCase):
  """Tests for cbuildbot_launch script."""

  def testPreParseArguments(self):
    """Test that we can correctly extract branch values from cbuildbot args."""
    CASES = (
        (['--buildroot', '/buildroot', 'daisy-incremental'],
         (None, '/buildroot', None)),

        (['--buildbot', '--buildroot', '/buildroot',
          '--git-cache-dir', '/git-cache',
          '-b', 'release-R57-9202.B',
          'daisy-incremental'],
         ('release-R57-9202.B', '/buildroot', '/git-cache')),

        (['--debug', '--buildbot', '--notests',
          '--buildroot', '/buildroot',
          '--git-cache-dir', '/git-cache',
          '--branch', 'release-R57-9202.B',
          'daisy-incremental'],
         ('release-R57-9202.B', '/buildroot', '/git-cache')),
    )

    for cmd_args, expected in CASES:
      expected_branch, expected_buildroot, expected_cache_dir = expected

      options = cbuildbot_launch.PreParseArguments(cmd_args)

      self.assertEqual(options.branch, expected_branch)
      self.assertEqual(options.buildroot, expected_buildroot)
      self.assertEqual(options.git_cache_dir, expected_cache_dir)

  def testInitialCheckout(self):
    """Test InitialCheckout with minimum settings."""
    mock_repo = mock.MagicMock()
    mock_repo.branch = 'branch'

    cbuildbot_launch.InitialCheckout(mock_repo)

    self.assertEqual(mock_repo.mock_calls, [
        mock.call.Sync(detach=True),
    ])

  def testConfigureGlobalEnvironment(self):
    """Ensure that we can setup our global runtime environment correctly."""

    os.environ.pop('LANG', None)
    os.environ['LC_MONETARY'] = 'bad'

    cbuildbot_launch.ConfigureGlobalEnvironment()

    # Verify umask is updated.
    self.assertEqual(os.umask(0), 0o22)

    # Verify ENVs are cleaned up.
    self.assertEqual(os.environ['LANG'], 'en_US.UTF-8')
    self.assertNotIn('LC_MONETARY', os.environ)


class RunDepotToolsEnsureBootstrap(cros_build_lib_unittest.RunCommandTestCase,
                                   cros_test_lib.TempDirTestCase):
  """Test the helper function DepotToolsEnsureBootstrap."""

  def testEnsureBootstrap(self):
    """Verify that the script is run if present."""
    script = os.path.join(self.tempdir, 'ensure_bootstrap')
    osutils.Touch(script, makedirs=True)

    cbuildbot_launch.DepotToolsEnsureBootstrap(self.tempdir)
    self.assertCommandCalled(
        [script], extra_env={'PATH': mock.ANY}, cwd=self.tempdir)

  def testEnsureBootstrapMissing(self):
    """Verify that the script is NOT run if not present."""
    cbuildbot_launch.DepotToolsEnsureBootstrap(self.tempdir)
    self.assertEqual(self.rc.call_count, 0)


class RunTests(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for cbuildbot_launch script."""

  ARGS_BASE = ['--buildroot', '/buildroot']
  EXPECTED_ARGS_BASE = ['--buildroot', '/cbuildbot_buildroot']
  ARGS_GIT_CACHE = ['--git-cache-dir', '/git-cache']
  ARGS_CONFIG = ['config']
  CMD = ['/cbuildbot_buildroot/chromite/bin/cbuildbot']

  def verifyCbuildbot(self, args, expected_cmd, version):
    """Ensure we invoke cbuildbot correctly."""
    self.PatchObject(
        cros_build_lib, 'GetTargetChromiteApiVersion', autospec=True,
        return_value=version)

    cbuildbot_launch.Cbuildbot('/cbuildbot_buildroot', '/depot_tools', args)

    self.assertCommandCalled(
        expected_cmd, extra_env={'PATH': mock.ANY},
        cwd='/cbuildbot_buildroot', error_code_ok=True)

  def testCbuildbotSimple(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG,
        self.CMD + self.ARGS_CONFIG + self.EXPECTED_ARGS_BASE,
        (0, 4))

  def testCbuildbotNotFiltered(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG + self.ARGS_GIT_CACHE,
        (self.CMD + self.ARGS_CONFIG + self.EXPECTED_ARGS_BASE +
         self.ARGS_GIT_CACHE),
        (0, 4))

  def testCbuildbotFiltered(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG + self.ARGS_GIT_CACHE,
        self.CMD + self.ARGS_CONFIG + self.EXPECTED_ARGS_BASE,
        (0, 2))

  def testMainMin(self):
    """Test a minimal set of command line options."""
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)
    self.PatchObject(cros_build_lib, 'GetTargetChromiteApiVersion',
                     autospec=True, return_value=(constants.REEXEC_API_MAJOR,
                                                  constants.REEXEC_API_MINOR))
    mock_repo = mock.MagicMock()
    mock_repo.branch = 'master'
    mock_repo.directory = '/root/repository'

    mock_repo_create = self.PatchObject(repository, 'RepoRepository',
                                        autospec=True, return_value=mock_repo)
    mock_clean = self.PatchObject(cbuildbot_launch, 'CleanBuildRoot',
                                  autospec=True)
    mock_checkout = self.PatchObject(cbuildbot_launch, 'InitialCheckout',
                                     autospec=True)
    mock_cleanup_chroot = self.PatchObject(cbuildbot_launch, 'CleanupChroot',
                                           autospec=True)

    cbuildbot_launch._main(['-r', '/root', 'config'])

    # Did we create the repo instance correctly?
    self.assertEqual(mock_repo_create.mock_calls,
                     [mock.call(EXPECTED_MANIFEST_URL, '/root/repository',
                                git_cache_dir=None, branch='master')])

    # Ensure we clean, as expected.
    self.assertEqual(mock_clean.mock_calls, [
        mock.call('/root', mock_repo,
                  {
                      'branch_name': 'master',
                      'tryjob': False,
                      'build_config': 'config',
                  })])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call(mock_repo)])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        [
            '/root/repository/chromite/bin/cbuildbot',
            'config',
            '-r', '/root/repository',
            '--ts-mon-task-num', '1',
        ],
        extra_env={'PATH': mock.ANY},
        cwd='/root/repository',
        error_code_ok=True)

    # Ensure we clean the chroot, as expected.
    self.assertEqual(mock_cleanup_chroot.mock_calls, [
        mock.call('/root/repository')])

  def testMainMax(self):
    """Test a larger set of command line options."""
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)
    self.PatchObject(cros_build_lib, 'GetTargetChromiteApiVersion',
                     autospec=True, return_value=(constants.REEXEC_API_MAJOR,
                                                  constants.REEXEC_API_MINOR))
    mock_repo = mock.MagicMock()
    mock_repo.branch = 'branch'
    mock_repo.directory = '/root/repository'

    mock_repo_create = self.PatchObject(repository, 'RepoRepository',
                                        autospec=True, return_value=mock_repo)
    mock_clean = self.PatchObject(cbuildbot_launch, 'CleanBuildRoot',
                                  autospec=True)
    mock_checkout = self.PatchObject(cbuildbot_launch, 'InitialCheckout',
                                     autospec=True)
    mock_cleanup_chroot = self.PatchObject(cbuildbot_launch, 'CleanupChroot',
                                           autospec=True)

    cbuildbot_launch._main(['--buildroot', '/root',
                            '--branch', 'branch',
                            '--git-cache-dir', '/git-cache',
                            '--remote-trybot',
                            'config'])

    # Did we create the repo instance correctly?
    self.assertEqual(mock_repo_create.mock_calls,
                     [mock.call(EXPECTED_MANIFEST_URL, '/root/repository',
                                git_cache_dir='/git-cache', branch='branch')])

    # Ensure we clean, as expected.
    self.assertEqual(mock_clean.mock_calls, [
        mock.call('/root',
                  mock_repo,
                  {
                      'branch_name': 'branch',
                      'tryjob': True,
                      'build_config': 'config',
                  })])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call(mock_repo)])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        [
            '/root/repository/chromite/bin/cbuildbot',
            'config',
            '--buildroot', '/root/repository',
            '--branch', 'branch',
            '--git-cache-dir', '/git-cache',
            '--remote-trybot',
            '--ts-mon-task-num', '1',
        ],
        extra_env={'PATH': mock.ANY},
        cwd='/root/repository',
        error_code_ok=True)

    # Ensure we clean the chroot, as expected.
    self.assertEqual(mock_cleanup_chroot.mock_calls, [
        mock.call('/root/repository')])


class CleanBuildRootTest(cros_test_lib.MockTempDirTestCase):
  """Tests for CleanBuildRoot method."""

  def setUp(self):
    """Create standard buildroot contents for cleanup."""
    self.root = os.path.join(self.tempdir)
    self.state = os.path.join(self.root, '.cbuildbot_launch_state')
    self.buildroot = os.path.join(self.root, 'buildroot')
    self.repo = os.path.join(self.buildroot, '.repo/repo')
    self.chroot = os.path.join(self.buildroot, 'chroot/chroot')
    self.general = os.path.join(self.buildroot, 'general/general')
    self.cache = os.path.join(self.buildroot, '.cache')
    self.distfiles = os.path.join(self.cache, 'distfiles')

    self.mock_repo = mock.MagicMock()
    self.mock_repo.directory = self.buildroot

    self.metrics = {}

  def populateBuildroot(self, state=None):
    """Create standard buildroot contents for cleanup."""
    if state:
      osutils.SafeMakedirs(self.root)
      osutils.WriteFile(self.state, state)

    # Create files.
    for f in (self.repo, self.chroot, self.general, self.distfiles):
      osutils.Touch(f, makedirs=True)

  def testNoBuildroot(self):
    """Test CleanBuildRoot with no history."""
    self.mock_repo.branch = 'master'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'master')
    self.assertIsNotNone(distfiles_ts)

  def testBuildrootNoState(self):
    """Test CleanBuildRoot with no state information."""
    self.populateBuildroot()
    self.mock_repo.branch = 'master'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'master')
    self.assertIsNotNone(distfiles_ts)

    self.assertNotExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertNotExists(self.general)
    self.assertNotExists(self.distfiles)

  def testBuildrootFormatMismatch(self):
    """Test CleanBuildRoot with no state information."""
    self.populateBuildroot('0 master')
    self.mock_repo.branch = 'master'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'master')
    self.assertIsNotNone(distfiles_ts)

    self.assertNotExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertNotExists(self.general)
    self.assertNotExists(self.distfiles)

  def testBuildrootBranchChange(self):
    """Test CleanBuildRoot with a change in branches."""
    self.populateBuildroot('2 branchA')
    self.mock_repo.branch = 'branchB'
    m = self.PatchObject(cros_build_lib, 'CleanupChrootMount')

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'branchB')
    self.assertIsNotNone(distfiles_ts)

    self.assertExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertExists(self.general)
    self.assertNotExists(self.distfiles)
    m.assert_called()

  def testBuildrootBranchMatch(self):
    """Test CleanBuildRoot with no change in branch."""
    self.populateBuildroot('2 branchA')
    self.mock_repo.branch = 'branchA'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'branchA')
    self.assertIsNotNone(distfiles_ts)

    self.assertExists(self.repo)
    self.assertExists(self.chroot)
    self.assertExists(self.general)
    self.assertExists(self.distfiles)

  def testBuildrootDistfilesRecentCache(self):
    """Test CleanBuildRoot does not delete distfiles when cache is recent."""
    seed_distfiles_ts = time.time() - 60
    self.populateBuildroot('2 branchA %f' % seed_distfiles_ts)
    self.mock_repo.branch = 'branchA'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'branchA')
    # Same cache creation timestamp is rewritten to state.
    self.assertEqual(distfiles_ts, seed_distfiles_ts)

    self.assertExists(self.repo)
    self.assertExists(self.chroot)
    self.assertExists(self.general)
    self.assertExists(self.distfiles)

  def testBuildrootDistfilesCacheExpired(self):
    """Test CleanBuildRoot when the distfiles cache is too old."""
    self.populateBuildroot('2 branchA 100.000000')
    self.mock_repo.branch = 'branchA'

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'branchA')
    self.assertIsNotNone(distfiles_ts)

    self.assertExists(self.repo)
    self.assertExists(self.chroot)
    self.assertExists(self.general)
    self.assertNotExists(self.distfiles)

  def testBuildrootRepoCleanFailure(self):
    """Test CleanBuildRoot with repo checkout failure."""
    self.populateBuildroot('1 branchA')
    self.mock_repo.branch = 'branchA'
    self.mock_repo.BuildRootGitCleanup.side_effect = Exception

    cbuildbot_launch.CleanBuildRoot(
        self.root, self.mock_repo, self.metrics)

    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'branchA')
    self.assertIsNotNone(distfiles_ts)

    self.assertNotExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertNotExists(self.general)
    self.assertNotExists(self.distfiles)

  def testGetState(self):
    """Test GetState."""
    # No root dir.
    results = cbuildbot_launch.GetState(self.root)
    self.assertEqual(results, (0, '', None))

    # Empty root dir.
    osutils.SafeMakedirs(self.root)
    results = cbuildbot_launch.GetState(self.root)
    self.assertEqual(results, (0, '', None))

    # Empty contents
    osutils.WriteFile(self.state, '')
    results = cbuildbot_launch.GetState(self.root)
    self.assertEqual(results, (0, '', None))

    # Old format contents
    osutils.WriteFile(self.state, 'happy-branch')
    results = cbuildbot_launch.GetState(self.root)
    self.assertEqual(results, (0, '', None))

    # Expected contents, without distfiles timestamp
    osutils.WriteFile(self.state, '1 happy-branch')
    results = cbuildbot_launch.GetState(self.root)
    self.assertEqual(results, (1, 'happy-branch', None))

    # Expected contents
    osutils.WriteFile(self.state, '1 happy-branch 1000.33')
    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 1)
    self.assertEqual(branch, 'happy-branch')
    self.assertEqual(distfiles_ts, 1000.33)

    # Future layout version contents
    osutils.WriteFile(self.state, '22 happy-branch 222')
    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 22)
    self.assertEqual(branch, 'happy-branch')
    self.assertEqual(distfiles_ts, 222)

    # Read write
    cbuildbot_launch.SetState('happy-branch', self.root)
    version, branch, distfiles_ts = cbuildbot_launch.GetState(self.root)
    self.assertEqual(version, 2)
    self.assertEqual(branch, 'happy-branch')
    self.assertIsNotNone(distfiles_ts)

  def testSetState(self):
    """Test SetState."""
    # Write out a state file.
    osutils.SafeMakedirs(self.root)
    cbuildbot_launch.SetState('happy-branch', self.root)
    state_file_parts = osutils.ReadFile(self.state).split()
    self.assertEqual(state_file_parts[:2], ['2', 'happy-branch'])
    # Will flake if this test takes > 1 hour to run.
    self.assertGreater(float(state_file_parts[2]), time.time() - 3600)

    # Explicitly provide a timestamp
    cbuildbot_launch.SetState('happy-branch', self.root,
                              333.33)
    state_file_parts = osutils.ReadFile(self.state).split()
    self.assertEqual(state_file_parts,
                     ['2', 'happy-branch', '333.330000'])

    # Change to a future version.
    self.PatchObject(cbuildbot_launch, 'BUILDROOT_BUILDROOT_LAYOUT', 22)
    cbuildbot_launch.SetState('happy-branch', self.root)
    state_file_parts = osutils.ReadFile(self.state).split()
    self.assertEqual(state_file_parts[:2], ['22', 'happy-branch'])
    # Will flake if this test takes > 1 hour to run.
    self.assertGreater(float(state_file_parts[2]), time.time() - 3600)

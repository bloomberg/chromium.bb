# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import bootstrap

# pylint
EXPECTED_MANIFEST_URL = 'https://chrome-internal-review.googlesource.com/chromeos/manifest-internal'  # pylint: disable=line-too-long


class BootstrapTest(cros_test_lib.MockTestCase):
  """Tests for bootstrap script."""

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

      options = bootstrap.PreParseArguments(cmd_args)

      self.assertEqual(options.branch, expected_branch)
      self.assertEqual(options.buildroot, expected_buildroot)
      self.assertEqual(options.git_cache_dir, expected_cache_dir)

  def testInitialCheckoutMin(self):
    """Test InitialCheckout with minimum settings."""
    mock_repo = self.PatchObject(repository, 'RepoRepository', autospec=True)

    bootstrap.InitialCheckout(None, '/buildroot', None)

    self.assertEqual(mock_repo.mock_calls, [
        mock.call(EXPECTED_MANIFEST_URL, '/buildroot',
                  branch=None, git_cache_dir=None),
        mock.call().Sync()
    ])

  def testInitialCheckoutMax(self):
    """Test InitialCheckout with all settings."""
    mock_repo = self.PatchObject(repository, 'RepoRepository', autospec=True)

    bootstrap.InitialCheckout('release-R56-9000.B', '/buildroot', '/git-cache')

    self.assertEqual(mock_repo.mock_calls, [
        mock.call(EXPECTED_MANIFEST_URL, '/buildroot',
                  branch='release-R56-9000.B', git_cache_dir='/git-cache'),
        mock.call().Sync()
    ])


class RunTests(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for bootstrap script."""

  ARGS_BASE = ['--buildroot', '/buildroot']
  ARGS_GIT_CACHE = ['--git-cache-dir', '/git-cache']
  ARGS_CONFIG = ['config']
  CMD = ['/buildroot/chromite/bin/cbuildbot']

  def verifyRunCbuildbot(self, args, expected_cmd, version):
    """Ensure we invoke cbuildbot correctly."""
    options = bootstrap.PreParseArguments(args)

    self.PatchObject(
        cros_build_lib, 'GetTargetChromiteApiVersion', autospec=True,
        return_value=version)

    bootstrap.RunCbuildbot(options)

    self.assertCommandCalled(
        expected_cmd, cwd=options.buildroot, error_code_ok=True)

  def testRunCbuildbotSimple(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyRunCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG,
        self.CMD + self.ARGS_CONFIG + self.ARGS_BASE,
        (0, 4))

  def testRunCbuildbotNotFiltered(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyRunCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG + self.ARGS_GIT_CACHE,
        self.CMD + self.ARGS_CONFIG + self.ARGS_BASE + self.ARGS_GIT_CACHE,
        (0, 4))

  def testRunCbuildbotFiltered(self):
    """Ensure we invoke cbuildbot correctly."""
    self.verifyRunCbuildbot(
        self.ARGS_BASE + self.ARGS_CONFIG + self.ARGS_GIT_CACHE,
        self.CMD + self.ARGS_CONFIG + self.ARGS_BASE,
        (0, 2))

  def testMainMin(self):
    """Test a minimal set of command line options."""
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)
    self.PatchObject(cros_build_lib, 'GetTargetChromiteApiVersion',
                     autospec=True, return_value=(0, 4))
    mock_clean = self.PatchObject(bootstrap, 'CleanBuildroot', autospec=True)
    mock_checkout = self.PatchObject(bootstrap, 'InitialCheckout',
                                     autospec=True)

    bootstrap.main(['--buildroot', '/buildroot', 'config'])

    # Ensure we clean, as expected.
    self.assertEqual(mock_clean.mock_calls,
                     [mock.call(None, '/buildroot')])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call(None, '/buildroot', None)])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        ['/buildroot/chromite/bin/cbuildbot',
         'config', '--buildroot', '/buildroot'],
        cwd='/buildroot', error_code_ok=True)

  def testMainMax(self):
    """Test a larger set of command line options."""
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)
    self.PatchObject(cros_build_lib, 'GetTargetChromiteApiVersion',
                     autospec=True, return_value=(0, 4))
    mock_clean = self.PatchObject(bootstrap, 'CleanBuildroot', autospec=True)
    mock_checkout = self.PatchObject(bootstrap, 'InitialCheckout',
                                     autospec=True)

    bootstrap.main(['--buildroot', '/buildroot',
                    '--git-cache-dir', '/git-cache',
                    'config'])

    # Ensure we clean, as expected.
    self.assertEqual(mock_clean.mock_calls,
                     [mock.call(None, '/buildroot')])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call(None, '/buildroot', '/git-cache')])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        ['/buildroot/chromite/bin/cbuildbot',
         'config',
         '--buildroot', '/buildroot',
         '--git-cache-dir', '/git-cache'],
        cwd='/buildroot', error_code_ok=True)


class CleanBuildrootTest(cros_test_lib.TempDirTestCase):
  """Tests for CleanBuildroot method."""

  def setUp(self):
    """Create standard buildroot contents for cleanup."""
    self.state = os.path.join(self.tempdir, '.bootstrap_state')
    self.repo = os.path.join(self.tempdir, '.repo/repo')
    self.chroot = os.path.join(self.tempdir, 'chroot/chroot')
    self.general = os.path.join(self.tempdir, 'general/general')
    # TODO: Add .cache, and distfiles.

  def populateBuildroot(self, state=None):
    """Create standard buildroot contents for cleanup."""
    if state:
      osutils.WriteFile(self.state, state)

    # Create files.
    for f in (self.repo, self.chroot, self.general):
      osutils.Touch(os.path.join(self.tempdir, f), makedirs=True)

  def testBuildrootEmpty(self):
    """Test CleanBuildroot with no history."""
    bootstrap.CleanBuildroot(None, self.tempdir)

    self.assertEqual(osutils.ReadFile(self.state), 'TOT')

  def testBuildrootNoState(self):
    """Test CleanBuildroot with no state information."""
    self.populateBuildroot()

    bootstrap.CleanBuildroot(None, self.tempdir)

    self.assertEqual(osutils.ReadFile(self.state), 'TOT')
    self.assertExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertExists(self.general)

  def testBuildrootBranchChange(self):
    """Test CleanBuildroot with a change in branches."""
    self.populateBuildroot('branchA')

    bootstrap.CleanBuildroot('branchB', self.tempdir)

    self.assertEqual(osutils.ReadFile(self.state), 'branchB')
    self.assertExists(self.repo)
    self.assertNotExists(self.chroot)
    self.assertExists(self.general)

  def testBuildrootBranchMatch(self):
    """Test CleanBuildroot with no change in branch."""
    self.populateBuildroot('branchA')

    bootstrap.CleanBuildroot('branchA', self.tempdir)

    self.assertEqual(osutils.ReadFile(self.state), 'branchA')
    self.assertExists(self.repo)
    self.assertExists(self.chroot)
    self.assertExists(self.general)

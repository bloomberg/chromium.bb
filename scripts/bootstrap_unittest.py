# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from __future__ import print_function

import mock

from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib_unittest
from chromite.lib import osutils
from chromite.scripts import bootstrap

# pylint
EXPECTED_MANIFEST_URL = 'https://chrome-internal-review.googlesource.com/chromeos/manifest-internal'  # pylint: disable=line-too-long


class BootstrapTest(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for bootstrap script."""

  def testPreParseArguments(self):
    """Test that we can correctly extract branch values from cbuildbot args."""
    cases = (
        (['--buildroot', '/build'],
         None, '/build', None),
        (['--branch', 'branch', '-r', '/build'],
         'branch', '/build', None),
        (['-r', '/build', '-b', 'branch', 'config'],
         'branch', '/build', None),
    )

    for args, expected_branch, expected_root, expected_git_cache in cases:
      result = bootstrap.PreParseArguments(args)
      self.assertEqual(result.branch, expected_branch)
      self.assertEqual(result.buildroot, expected_root)
      self.assertEqual(result.git_cache_dir, expected_git_cache)

  def testInitialCheckoutMin(self):
    """Test InitialCheckout with minimum settings."""
    mock_repo = self.PatchObject(repository, 'RepoRepository', autospec=True)
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)

    bootstrap.InitialCheckout(None, '/buildroot', None)

    self.assertEqual(mock_repo.mock_calls, [
        mock.call(EXPECTED_MANIFEST_URL, '/buildroot',
                  branch=None, git_cache_dir=None),
        mock.call().Sync()
    ])

  def testInitialCheckoutMax(self):
    """Test InitialCheckout with all settings."""
    mock_repo = self.PatchObject(repository, 'RepoRepository', autospec=True)
    self.PatchObject(osutils, 'SafeMakedirs', autospec=True)

    bootstrap.InitialCheckout('release-R56-9000.B', '/buildroot', '/git-cache')

    self.assertEqual(mock_repo.mock_calls, [
        mock.call(EXPECTED_MANIFEST_URL, '/buildroot',
                  branch='release-R56-9000.B', git_cache_dir='/git-cache'),
        mock.call().Sync()
    ])

  def testRunCbuildbot(self):
    """Ensure we invoke cbuildbot correctly."""
    bootstrap.RunCbuildbot('/buildroot', ['foo', 'bar', 'arg'])
    self.assertCommandContains(
        ['/buildroot/chromite/bin/cbuildbot', 'foo', 'bar', 'arg'])

  def testMainMin(self):
    """Test a minimal set of command line options."""
    mock_checkout = self.PatchObject(bootstrap, 'InitialCheckout',
                                     autospec=True)

    bootstrap.main(['--buildroot', '/buildroot', 'foo'])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call(None, '/buildroot', None)])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        ['/buildroot/chromite/bin/cbuildbot',
         '--buildroot', '/buildroot', 'foo'],
        cwd='/buildroot', error_code_ok=True)

  def testMainMax(self):
    """Test a maximal set of command line options."""
    mock_checkout = self.PatchObject(bootstrap, 'InitialCheckout',
                                     autospec=True)

    bootstrap.main(['--buildroot', '/buildroot', '--branch', 'branch',
                    '--git-cache-dir', '/git-cache', 'foo'])

    # Ensure we checkout, as expected.
    self.assertEqual(mock_checkout.mock_calls,
                     [mock.call('branch', '/buildroot', '/git-cache')])

    # Ensure we invoke cbuildbot, as expected.
    self.assertCommandCalled(
        ['/buildroot/chromite/bin/cbuildbot',
         '--buildroot', '/buildroot', '--branch', 'branch',
         '--git-cache-dir', '/git-cache', 'foo'],
        cwd='/buildroot', error_code_ok=True)

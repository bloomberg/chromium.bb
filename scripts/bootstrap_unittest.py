# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from __future__ import print_function

from chromite.scripts import bootstrap
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib


class BootstrapTest(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for bootstrap script."""

  def testExtractBranchName(self):
    """Test that we can correctly extract branch values from cbuildbot args."""
    cases = (
        ([], 'master'),
        (['--arg1', 'value', '-a', '--arg2'], 'master'),
        (['--branch', 'branch'], 'branch'),
        (['-b', 'branch'], 'branch'),
        (['--arg1', 'value', '-a', '--branch', 'branch', '--arg2'], 'branch'),
        (['--arg1', '-a', '--branch', 'branch', 'config'], 'branch'),
    )

    for args, expected in cases:
      result = bootstrap.ExtractBranchName(args)
      self.assertEqual(result, expected)

  def testRunCbuildbot(self):
    bootstrap.RunCbuildbot('/mock/chromite', ['foo', 'bar', 'arg'])
    self.assertCommandContains(
        ['/mock/chromite/bin/cbuildbot', 'foo', 'bar', 'arg'])

  def testMainMaster(self):
    bootstrap.main(['foo'])

    # cbuildbot script is in tempdir, so can't be listed here.
    self.assertCommandContains(['foo'])

  def testMainBranch(self):
    bootstrap.main(['--branch', 'branch', 'foo'])

    # cbuildbot script is in tempdir, so can't be listed here.
    self.assertCommandContains(['--branch', 'branch', 'foo'])


class LiveWorktreeTest(cros_test_lib.TempDirTestCase):
  """Integration tests for git worktree usage."""

  @cros_test_lib.NetworkTest()
  def testCreateChromiteBranch(self):
    """Test that we can a git worktree, from the current chromite, branched."""
    bootstrap.CloneChromiteOnBranch('release-R56-9000.B', self.tempdir)

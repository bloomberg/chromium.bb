# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_mojo_as_stable.py."""

from __future__ import print_function

import datetime
import mock
import os

from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.scripts import cros_mark_as_stable
from chromite.scripts import cros_mark_mojo_as_stable


class CrosMarkChromeAsStableTestCase(cros_test_lib.MockTempDirTestCase):
  """Various test helpers for cros_mark_mojo_as_stable."""

  def setUp(self):
    """Setup variables and create mock dir."""
    self.tmp_overlay = os.path.join(self.tempdir, 'chromiumos-overlay')
    self.fake_ebuild_dir = os.path.join(self.tmp_overlay,
                                        cros_mark_mojo_as_stable.MOJO_CP)
    osutils.SafeMakedirs(self.fake_ebuild_dir)

  def WriteFile(self, filename, data=None):
    """Writes data to a file in the fake ebuild directory.

    Args:
      filename: Name of file to write to.
      data: Data to write or None for an empty file.
    """
    osutils.WriteFile(os.path.join(self.fake_ebuild_dir, filename), data or '')


class GetStableEbuildsTest(CrosMarkChromeAsStableTestCase):
  """Tests for GetStableEBuilds()."""

  def testBasic(self):
    """Basic sanity check."""
    self.WriteFile('foo-9999.ebuild')
    self.WriteFile('foo-42-r1.ebuild')
    self.WriteFile('foo-43-r1.ebuild')
    self.WriteFile('foo-44-r1.ebuild')
    ebuilds = cros_mark_mojo_as_stable.GetStableEBuilds(self.fake_ebuild_dir)
    self.assertEqual(len(ebuilds), 3)
    self.assertIn('foo-42-r1.ebuild', ebuilds)
    self.assertIn('foo-43-r1.ebuild', ebuilds)
    self.assertIn('foo-44-r1.ebuild', ebuilds)


class UprevStableEBuildTest(CrosMarkChromeAsStableTestCase):
  """Tests for UprevStableEBuild()."""

  def testSameCommit(self):
    """Check we do nothing if there is no new commit."""
    git_mock = self.PatchObject(git, 'RunGit')
    git_branch_mock = self.PatchObject(cros_mark_as_stable, 'GitBranch')
    commit_mock = self.PatchObject(portage_util.EBuild, 'CommitChange')

    self.WriteFile('mojo-9999.ebuild')
    self.WriteFile('mojo-0.20141205.164445-r1.ebuild')
    commit_date = datetime.datetime(2014, 12, 5, 16, 44, 45)
    atom = cros_mark_mojo_as_stable.UprevStableEBuild(self.fake_ebuild_dir,
                                                      '1234ab', commit_date)
    self.assertEqual(atom, None)
    self.assertEqual(git_mock.call_count, 0)
    self.assertEqual(git_branch_mock.call_count, 0)
    self.assertEqual(commit_mock.call_count, 0)

  def testNewCommit(self):
    """Check we do the right thing if there is a new commit.

    If there is a new commit, a new stable ebuild should be created and the
    old ones should be deleted.
    """
    git_mock = self.PatchObject(git, 'RunGit')
    git_branch_mock = self.PatchObject(cros_mark_as_stable, 'GitBranch')
    commit_mock = self.PatchObject(portage_util.EBuild, 'CommitChange')

    self.WriteFile('mojo-9999.ebuild')
    self.WriteFile('mojo-0.20141204.120042-r1.ebuild')
    self.WriteFile('mojo-0.20141205.164445-r1.ebuild')
    commit_date = datetime.datetime(2014, 12, 6, 5, 0, 0)
    atom = cros_mark_mojo_as_stable.UprevStableEBuild(self.fake_ebuild_dir,
                                                      '1234ab', commit_date)
    self.assertEqual(atom, 'dev-libs/mojo-0.20141206.050000-r1')

    git_branch_mock.assert_called_once_with(
        'stabilizing_branch', 'remotes/m/master',
        self.fake_ebuild_dir)

    # The 'git rm -f' calls could be in any order because
    # GetStableEBuilds() does not guarantee the order.
    git_mock.assert_has_calls([
        mock.call(self.fake_ebuild_dir,
                  ['add', os.path.join(self.fake_ebuild_dir,
                                       'mojo-0.20141206.050000-r1.ebuild')]),
        mock.call(self.fake_ebuild_dir,
                  ['rm', '-f', 'mojo-0.20141204.120042-r1.ebuild']),
        mock.call(self.fake_ebuild_dir,
                  ['rm', '-f', 'mojo-0.20141205.164445-r1.ebuild']),
    ], any_order=True)

    commit_mock.assert_called_once_with(
        'Updated dev-libs/mojo to upstream commit 1234ab.',
        self.fake_ebuild_dir)

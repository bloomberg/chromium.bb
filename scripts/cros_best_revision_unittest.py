# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cros_best_revision program."""

from __future__ import print_function

import os
import time

from chromite.lib import builder_status_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import tree_status
from chromite.scripts import cros_best_revision


class BaseChromeCommitterTest(cros_test_lib.MockTempDirTestCase):
  """Base class for tests using cros_best_revision.ChromeCommitter."""

  def setUp(self):
    """Common set up method for all tests."""
    self.committer = cros_best_revision.ChromeCommitter(self.tempdir, False)
    self.lkgm_file = os.path.join(self.tempdir, constants.PATH_TO_CHROME_LKGM)
    self.pass_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)
    self.fail_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, None)
    # No need to make tests sleep.
    self.PatchObject(time, 'sleep')


# pylint: disable=W0212
class ChromeGSTest(BaseChromeCommitterTest):
  """Test cros_best_revision.ChromeCommitter version filtering."""

  def testGetLatestCanaryVersions(self):
    """Test that we correctly filter out non-canary and older versions."""
    output = '\n'.join(['2910.0.1', '2900.0.0', '2908.0.0', '2909.0.0',
                        '2910.0.0'])
    # Only return 2 -- the 2 newest canary results.
    cros_best_revision.ChromeCommitter._CANDIDATES_TO_CONSIDER = 2
    expected_output = ['2910.0.0', '2909.0.0']

    self.committer._old_lkgm = '2905.0.0'
    with gs_unittest.GSContextMock() as gs_mock:
      gs_mock.AddCmdResult(partial_mock.In('ls'), output=output)
      versions = self.committer._GetLatestCanaryVersions()
    self.assertEqual(versions, expected_output)


class ChromeCommitterTester(cros_build_lib_unittest.RunCommandTestCase,
                            BaseChromeCommitterTest):
  """Test cros_best_revision.Committer."""

  canaries = ['a-release', 'b-release', 'c-release']
  versions = ['4.0.0', '3.0.0']

  def testCheckoutChromeLKGM(self):
    "Tests that we can read/obtain the old LKGM from mocked out SVN."
    # Write out an old lkgm file as if we got it from svn update.
    old_lkgm = '2098.0.0'
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    osutils.WriteFile(self.lkgm_file, old_lkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, old_lkgm)

  def _TestFindNewLKGM(self, all_results, lkgm):
    """Stubs out methods used by FindNewLKGM."""
    expected = {}
    for canary, results in zip(self.canaries, all_results):
      for version, status in zip(self.versions, results):
        expected[(canary, version)] = status
    def _GetBuilderStatus(canary, version, **_):
      return expected[(canary, version)]
    self.PatchObject(self.committer, '_GetLatestCanaryVersions',
                     return_value=self.versions)
    self.PatchObject(self.committer, 'GetCanariesForChromeLKGM',
                     return_value=self.canaries)
    self.PatchObject(builder_status_lib.BuilderStatusManager,
                     'GetBuilderStatus',
                     side_effect=_GetBuilderStatus)
    self.committer.FindNewLKGM()
    self.assertTrue(self.committer._lkgm, lkgm)

  def testFindNewLKGMBasic(self):
    """Tests that we return the highest version if all versions are good."""
    self._TestFindNewLKGM([[self.pass_status] * 2] * 3, '4.0.0')

  def testFindNewLKGMAdvanced(self):
    """Tests that we return the only version with passing canaries."""
    self._TestFindNewLKGM([[self.fail_status, self.pass_status]] * 3, '3.0.0')

  def testFindNewLKGMWithFailures(self):
    """Ensure we reject versions with failed builds.

    This test case is a bit more complex than the two above and tests the logic
    where we want to reject versions with failed builds.

    In this example both versions have 2 passing builds. The older version
    is missing a score from one builder where the newer version reports
    a failure. In this instance, our scoring mechanism should choose the older
    version.
    """
    all_results = [[self.pass_status] * 2] * 2 + [[self.fail_status, None]]
    self._TestFindNewLKGM(all_results, '3.0.0')

  def testCommitNewLKGM(self):
    """Tests that we can commit a new LKGM file."""
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    self.committer._lkgm = '4.0.0'

    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)
    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    self.assertEqual(osutils.ReadFile(self.lkgm_file), self.committer._lkgm)
    self.assertCommandContains(['git', 'commit'])

  def testPushNewLKGM(self):
    """Tests that we can rebase if landing fails due to missing revisions."""
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)

    self.committer.PushNewLKGM()

    self.assertCommandContains(['git', 'cl', 'land'])

  def testPushNewLKGMWithRetry(self):
    """Tests that we try to rebase if landing fails due to missing revisions."""
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)

    self.rc.AddCmdResult(partial_mock.In('land'), returncode=1)
    self.assertRaises(cros_best_revision.LKGMNotCommitted,
                      self.committer.PushNewLKGM)

    self.assertCommandContains(['git', 'cl', 'land'])
    self.assertCommandContains(['git', 'fetch', 'origin', 'master'])
    self.assertCommandContains(['git', 'rebase'])

#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cros_best_revision program."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gclient
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import partial_mock

from chromite.scripts import cros_best_revision


class BaseChromeCommitterTest(cros_test_lib.MockTempDirTestCase):

  def setUp(self):
    """Common set up method for all tests."""
    self.committer = cros_best_revision.ChromeCommitter(self.tempdir, False)
    self.lkgm_file = os.path.join(self.tempdir, constants.CHROME_LKGM_FILE)
    self.pass_status = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_PASSED, None)
    self.fail_status = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_FAILED, None)


# pylint: disable=W0212
class ChromeGSTest(BaseChromeCommitterTest):

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

  canaries = ['a-release', 'b-release', 'c-release']
  versions = ['4.0.0', '3.0.0']

  def testCheckoutChromeLKGM(self):
    "Tests that we can read/obtain the old LKGM from mocked out SVN."
    # Write out an old lkgm file as if we got it from svn update.
    old_lkgm = '2098.0.0'
    osutils.WriteFile(self.lkgm_file, old_lkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, old_lkgm)
    self.assertCommandContains(['%s/%s' % (gclient.CHROME_COMMITTER_URL,
                                           constants.PATH_TO_CHROME_LKGM)])
    self.assertCommandContains([constants.CHROME_LKGM_FILE])

  def _TestFindNewLKGM(self, all_results, lkgm):
    """Stubs out methods used by FindNewLKGM."""
    expected = {}
    for canary, results in zip(self.canaries, all_results):
      for version, status in zip(self.versions, results):
        expected[(canary, version)] = status
    def _GetBuildStatus(canary, version, **_):
      return expected[(canary, version)]
    self.PatchObject(self.committer, '_GetLatestCanaryVersions',
                     return_value=self.versions)
    self.PatchObject(cbuildbot_config, 'GetCanariesForChromeLKGM',
                     return_value=self.canaries)
    self.PatchObject(manifest_version.BuildSpecsManager, 'GetBuildStatus',
                     side_effect=_GetBuildStatus)
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
    self.committer._lkgm = '4.0.0'
    self.PatchObject(cros_build_lib, 'TreeOpen', return_value=True)
    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    self.assertEqual(osutils.ReadFile(self.lkgm_file), self.committer._lkgm)
    self.assertCommandContains([constants.CHROME_LKGM_FILE])
    self.assertCommandContains(['commit'])


if __name__ == '__main__':
  cros_test_lib.main()

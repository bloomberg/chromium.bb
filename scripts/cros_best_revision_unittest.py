#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cros_best_revision program."""

import mox
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gclient
from chromite.lib import gs

from chromite.scripts import cros_best_revision


# pylint: disable=W0212,E1101,E1120
class ChromeCommitterTester(cros_test_lib.MoxTestCase):
  def setUp(self):
    """Common set up method for all tests."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.checkout_dir = tempfile.mkdtemp(prefix='cbr_chrome_checkout')
    self.committer = cros_best_revision.ChromeCommitter(self.checkout_dir,
                                                        False)
    self.old_lkgm = '2098.0.0'

  def tearDown(self):
    shutil.rmtree(self.checkout_dir)

  def testCheckoutChromeLKGM(self):
    "Tests that we can read/obtain the old LKGM from mocked out SVN."
    cros_build_lib.RunCommand(mox.In('%s/%s' % (gclient.CHROME_COMMITTER_URL,
                                                constants.PATH_TO_CHROME_LKGM)))
    cros_build_lib.RunCommand(mox.In(constants.CHROME_LKGM_FILE),
                              cwd=self.checkout_dir)

    self.mox.ReplayAll()

    # Write out an old lkgm file as if we got it from svn update.
    lkgm_file = os.path.join(self.checkout_dir, constants.CHROME_LKGM_FILE)
    with open(lkgm_file, 'w') as fh:
      fh.write(self.old_lkgm)

    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

  def testGetLatestCanaryVersions(self):
    """Test that we correctly filter out non-canary and older versions."""
    self.mox.StubOutWithMock(gs.GSContext, 'LS')
    return_obj = cros_build_lib.CommandResult
    return_obj.output = '\n'.join(['2910.0.1', '2900.0.0', '2908.0.0',
                                   '2909.0.0', '2910.0.0'])
    old_version = '2905.0.0'
    # Only return 2 -- the 2 newest canary results.
    cros_best_revision.ChromeCommitter._CANDIDATES_TO_CONSIDER = 2
    expected_output = ['2910.0.0', '2909.0.0']

    gs.GSContext.LS(manifest_version.BUILD_STATUS_URL).AndReturn(return_obj)

    self.mox.ReplayAll()

    self.committer._old_lkgm = old_version
    versions = self.committer._GetLatestCanaryVersions()
    self.assertEqual(versions, expected_output)

  def _CommonNewLKGMMocks(self):
    """Stubs out methods used by FindNewLKGM."""
    self.mox.StubOutWithMock(cbuildbot_config, 'GetCanariesForChromeLKGM')
    self.mox.StubOutWithMock(self.committer, '_GetLatestCanaryVersions')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetBuildStatus')

  def _GetPassFailStatuses(self):
    """Returns a tuple of pass/fail statuses."""
    pass_status = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_PASSED, None)
    fail_status = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_FAILED, None)
    return pass_status, fail_status


  def testFindNewLKGMBasic(self):
    """Tests that we return the highest version if all versions are good."""
    self._CommonNewLKGMMocks()

    pass_status, _ = self._GetPassFailStatuses()
    canaries = ['a-release', 'b-release', 'c-release']
    versions = ['4.0.0', '3.0.0', '2.0.0', '1.0.0']
    self.committer._GetLatestCanaryVersions().AndReturn(versions)
    cbuildbot_config.GetCanariesForChromeLKGM().AndReturn(canaries)
    manifest_version.BuildSpecsManager.GetBuildStatus(
        mox.IgnoreArg(), mox.IgnoreArg(), retries=0).MultipleTimes().AndReturn(
            pass_status)

    self.mox.ReplayAll()
    self.committer.FindNewLKGM()
    self.assertTrue(self.committer._lkgm, '4.0.0')

  def testFindNewLKGMAdvanced(self):
    """Tests that we return the only version with passing canaries."""
    self._CommonNewLKGMMocks()

    pass_status, fail_status = self._GetPassFailStatuses()
    canaries = ['a-release', 'b-release', 'c-release']
    versions = ['4.0.0', '3.0.0']
    self.committer._GetLatestCanaryVersions().AndReturn(versions)
    cbuildbot_config.GetCanariesForChromeLKGM().AndReturn(canaries)
    manifest_version.BuildSpecsManager.GetBuildStatus(
        mox.IgnoreArg(), '4.0.0', retries=0).MultipleTimes().AndReturn(
            fail_status)
    manifest_version.BuildSpecsManager.GetBuildStatus(
        mox.IgnoreArg(), '3.0.0', retries=0).MultipleTimes().AndReturn(
            pass_status)

    self.mox.ReplayAll()
    self.committer.FindNewLKGM()
    self.assertTrue(self.committer._lkgm, '3.0.0')

  def testFindNewLKGMWithFailures(self):
    """Ensure we reject versions with failed builds.

    This test case is a bit more complex than the two above and tests the logic
    where we want to reject versions with failed builds.

    In this example both versions have 2 passing builds. The older version
    is missing a score from one builder where the newer version reports
    a failure. In this instance, our scoring mechanism should choose the older
    version.
    """
    self._CommonNewLKGMMocks()

    pass_status, fail_status = self._GetPassFailStatuses()
    canaries = ['a-release', 'b-release', 'c-release']
    versions = ['4.0.0', '3.0.0']
    self.committer._GetLatestCanaryVersions().AndReturn(versions)
    cbuildbot_config.GetCanariesForChromeLKGM().AndReturn(canaries)
    for canary in canaries:
      if canary in ['a-release', 'b-release']:
        manifest_version.BuildSpecsManager.GetBuildStatus(
            canary, '4.0.0', retries=0).MultipleTimes().AndReturn(
                pass_status)
      else:
        manifest_version.BuildSpecsManager.GetBuildStatus(
            canary, '4.0.0', retries=0).MultipleTimes().AndReturn(
                fail_status)

    for canary in canaries:
      if canary in ['a-release', 'b-release']:
        manifest_version.BuildSpecsManager.GetBuildStatus(
            canary, '3.0.0', retries=0).MultipleTimes().AndReturn(
                pass_status)
      else:
        manifest_version.BuildSpecsManager.GetBuildStatus(
            canary, '3.0.0', retries=0).MultipleTimes().AndReturn(None)

    self.mox.ReplayAll()
    self.committer.FindNewLKGM()
    self.assertTrue(self.committer._lkgm, '3.0.0')

  def testCommitNewLKGM(self):
    """Tests that we can commit a new LKGM file."""
    self.committer._lkgm = '4.0.0'

    cros_build_lib.RunCommand(mox.In(constants.CHROME_LKGM_FILE),
                              cwd=self.checkout_dir)
    cros_build_lib.RunCommand(mox.In('commit'),
                              cwd=self.checkout_dir)

    self.mox.ReplayAll()
    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    lkgm_file = os.path.join(self.checkout_dir, constants.CHROME_LKGM_FILE)
    with open(lkgm_file) as fh:
      self.assertEqual(fh.read(), self.committer._lkgm)


if __name__ == '__main__':
  cros_test_lib.main()

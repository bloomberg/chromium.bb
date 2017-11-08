# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the chrome_chromeos_lkgm program."""

from __future__ import print_function

import os
import time

from chromite.lib import builder_status_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import tree_status
from chromite.scripts import chrome_chromeos_lkgm

# pylint: disable=protected-access

class BaseChromeLGTMCommitterTest(cros_test_lib.MockTempDirTestCase):
  """Base class for tests using cros_chromeos_lkgm.ChromeLGTMCommitter."""

  def setUp(self):
    """Common set up method for all tests."""
    self.committer = chrome_chromeos_lkgm.ChromeLGTMCommitter(
        self.tempdir, '1001.0.0', False, 'user@test.org')
    self.lkgm_file = os.path.join(self.tempdir, constants.PATH_TO_CHROME_LKGM)
    self.old_lkgm = None
    self.pass_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)
    self.fail_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, None)
    # No need to make tests sleep.
    self.PatchObject(time, 'sleep')


class ChromeLGTMCommitterTester(cros_build_lib_unittest.RunCommandTestCase,
                                BaseChromeLGTMCommitterTest):
  """Test cros_chromeos_lkgm.Committer."""

  def _createOldLkgm(self, items):  # pylint: disable=unused-argument
    # Write out an old lkgm file as if we got it from a git fetch.
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    osutils.WriteFile(self.lkgm_file, self.old_lkgm)

  def testCheckoutChromeLKGM(self):
    "Tests that we can read/obtain the old LKGM from mocked out git."
    self.old_lkgm = '1234.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

  def testCommitNewLKGM(self):
    """Tests that we can commit a new LKGM file."""
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    self.committer = chrome_chromeos_lkgm.ChromeLGTMCommitter(
        self.tempdir, '1002.0.0', False, 'user@test.org')

    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)
    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    self.assertEqual(osutils.ReadFile(self.lkgm_file), self.committer._lkgm)
    self.assertCommandContains(['git', 'commit'])

  def testLandNewLKGM(self):
    """Tests that we try to execute git cl land if the tree is open."""
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)

    self.committer.LandNewLKGM()

    self.assertCommandContains(['git', 'cl', 'land'])

  def testLandNewLKGMWithRetry(self):
    """Tests that we try to rebase if landing fails."""
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)

    self.rc.AddCmdResult(partial_mock.In('land'), returncode=1)
    self.assertRaises(chrome_chromeos_lkgm.LKGMNotCommitted,
                      self.committer.LandNewLKGM)

    self.assertCommandContains(['git', 'cl', 'land'])
    self.assertCommandContains(['git', 'fetch', 'origin', 'master'])
    self.assertCommandContains(['git', 'rebase'])

  def testOlderLKGMFails(self):
    """Tests that trying to update to an older lkgm version fails."""
    self.old_lkgm = '1002.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)

    self.committer = chrome_chromeos_lkgm.ChromeLGTMCommitter(
        self.tempdir, '1001.0.0', False, 'user@test.org')
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)
    self.assertRaises(chrome_chromeos_lkgm.LKGMNotValid,
                      self.committer.CommitNewLKGM)

  def testVersionWithChromeBranch(self):
    """Tests passing a version with a chrome branch strips the branch."""
    self.old_lkgm = '1002.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

    self.committer = chrome_chromeos_lkgm.ChromeLGTMCommitter(
        self.tempdir, '1003.0.0-rc2', False, 'user@test.org')

    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)
    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    stripped_lkgm = '1003.0.0'
    self.assertEqual(osutils.ReadFile(self.lkgm_file), stripped_lkgm)

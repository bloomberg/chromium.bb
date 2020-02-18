# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for clactions methods."""

from __future__ import print_function

import datetime
import random

from chromite.lib import constants
from chromite.lib import clactions
from chromite.lib import cros_test_lib


class TestCLActionHistorySmoke(cros_test_lib.TestCase):
  """A basic test for the simpler aggregating API for CLActionHistory."""

  def setUp(self):

    self.cl1 = clactions.GerritChangeTuple(11111, True)
    self.cl1_patch1 = clactions.GerritPatchTuple(
        self.cl1.gerrit_number, 1, self.cl1.internal)
    self.cl1_patch2 = clactions.GerritPatchTuple(
        self.cl1.gerrit_number, 2, self.cl1.internal)

    self.cl2 = clactions.GerritChangeTuple(22222, True)
    self.cl2_patch1 = clactions.GerritPatchTuple(
        self.cl2.gerrit_number, 1, self.cl2.internal)
    self.cl2_patch2 = clactions.GerritPatchTuple(
        self.cl2.gerrit_number, 2, self.cl2.internal)

    self.cl3 = clactions.GerritChangeTuple(33333, True)
    self.cl3_patch1 = clactions.GerritPatchTuple(
        self.cl3.gerrit_number, 2, self.cl3.internal)

    # Expected actions in chronological order, most recent first.
    self.action1 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl1_patch2, constants.CL_ACTION_SUBMITTED,
        timestamp=self._NDaysAgo(1))
    self.action2 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl1_patch2, constants.CL_ACTION_KICKED_OUT,
        timestamp=self._NDaysAgo(2))
    self.action3 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl2_patch2, constants.CL_ACTION_SUBMITTED,
        timestamp=self._NDaysAgo(3))
    self.action4 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl1_patch1, constants.CL_ACTION_SUBMIT_FAILED,
        timestamp=self._NDaysAgo(4))
    self.action5 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl1_patch1, constants.CL_ACTION_KICKED_OUT,
        timestamp=self._NDaysAgo(5))
    self.action6 = clactions.CLAction.FromGerritPatchAndAction(
        self.cl3_patch1, constants.CL_ACTION_SUBMITTED,
        reason=constants.STRATEGY_NONMANIFEST,
        timestamp=self._NDaysAgo(6))

    # CLActionHistory does not require the history to be given in chronological
    # order, so we provide them in reverse order, and expect them to be sorted
    # as appropriate.
    self.cl_action_stats = clactions.CLActionHistory([
        self.action1, self.action2, self.action3, self.action4, self.action5,
        self.action6])

  def _NDaysAgo(self, num_days):
    return datetime.datetime.today() - datetime.timedelta(num_days)

  def testAffected(self):
    """Tests that the Affected* methods DTRT."""
    self.assertEqual(set([self.cl1, self.cl2, self.cl3]),
                     self.cl_action_stats.affected_cls)
    self.assertEqual(
        set([self.cl1_patch1, self.cl1_patch2, self.cl2_patch2,
             self.cl3_patch1]),
        self.cl_action_stats.affected_patches)

  def testActions(self):
    """Tests that different types of actions are listed correctly."""
    self.assertEqual([self.action5, self.action2],
                     self.cl_action_stats.reject_actions)
    self.assertEqual([self.action6, self.action3, self.action1],
                     self.cl_action_stats.submit_actions)
    self.assertEqual([self.action4],
                     self.cl_action_stats.submit_fail_actions)

  def testSubmitted(self):
    """Tests that the list of submitted objects is correct."""
    self.assertEqual(set([self.cl1, self.cl2]),
                     self.cl_action_stats.GetSubmittedCLs())
    self.assertEqual(set([self.cl1, self.cl2, self.cl3]),
                     self.cl_action_stats.GetSubmittedCLs(False))
    self.assertEqual(set([self.cl1_patch2, self.cl2_patch2]),
                     self.cl_action_stats.GetSubmittedPatches())
    self.assertEqual(set([self.cl1_patch2, self.cl2_patch2, self.cl3_patch1]),
                     self.cl_action_stats.GetSubmittedPatches(False))


class TestCLActionHistoryRejections(cros_test_lib.TestCase):
  """Involved test of aggregation of rejections."""

  CQ_BUILD_CONFIG = 'lumpy-paladin'
  PRE_CQ_BUILD_CONFIG = 'pre-cq-group'

  def setUp(self):
    self._days_forward = 1
    self._build_id = 1
    self.action_history = []
    self.cl_action_stats = None

    self.cl1 = clactions.GerritChangeTuple(11111, True)
    self.cl1_patch1 = clactions.GerritPatchTuple(
        self.cl1.gerrit_number, 1, self.cl1.internal)
    self.cl1_patch2 = clactions.GerritPatchTuple(
        self.cl1.gerrit_number, 2, self.cl1.internal)

    self.cl2 = clactions.GerritChangeTuple(22222, True)
    self.cl2_patch1 = clactions.GerritPatchTuple(
        self.cl2.gerrit_number, 1, self.cl2.internal)
    self.cl2_patch2 = clactions.GerritPatchTuple(
        self.cl2.gerrit_number, 2, self.cl2.internal)

  def _AppendToHistory(self, patch, action, **kwargs):
    kwargs.setdefault('id', -1)
    kwargs.setdefault('build_id', -1)
    kwargs.setdefault('reason', '')
    kwargs.setdefault('build_config', '')
    kwargs['timestamp'] = (datetime.datetime.today() +
                           datetime.timedelta(self._days_forward))
    self._days_forward += 1
    kwargs['action'] = action
    kwargs['change_number'] = int(patch.gerrit_number)
    kwargs['patch_number'] = int(patch.patch_number)
    kwargs['change_source'] = clactions.BoolToChangeSource(patch.internal)
    kwargs['buildbucket_id'] = 'test-id'
    kwargs['status'] = None

    action = clactions.CLAction(**kwargs)
    self.action_history.append(action)
    return action

  def _PickupAndRejectPatch(self, patch, **kwargs):
    kwargs.setdefault('build_id', self._build_id)
    self._build_id += 1
    pickup_action = self._AppendToHistory(patch, constants.CL_ACTION_PICKED_UP,
                                          **kwargs)
    reject_action = self._AppendToHistory(patch, constants.CL_ACTION_KICKED_OUT,
                                          **kwargs)
    return pickup_action, reject_action

  def _CreateCLActionHistory(self):
    """Create the object under test, reordering the history.

    We reorder history in a fixed but arbitrary way, to test that order doesn't
    matter for the object under test.
    """
    random.seed(4)  # Everyone knows this is the randomest number on earth.
    random.shuffle(self.action_history)
    self.cl_action_stats = clactions.CLActionHistory(self.action_history)

  def testRejectionsNoRejection(self):
    """Tests the null case."""
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({}, self.cl_action_stats.GetTrueRejections())
    self.assertEqual({}, self.cl_action_stats.GetFalseRejections())

  def testTrueRejectionsSkipApplyFailure(self):
    """Test that apply failures are not considered true rejections."""
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_KICKED_OUT)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({}, self.cl_action_stats.GetTrueRejections())

  def testTrueRejectionsIncludeLaterSubmitted(self):
    """Tests that we include CLs which have a patch that was later submitted."""
    _, reject_action = self._PickupAndRejectPatch(self.cl1_patch1)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action]},
                     self.cl_action_stats.GetTrueRejections())

  def testTrueRejectionsMultipleRejectionsOnPatch(self):
    """Tests that we include all rejection actions on a patch."""
    _, reject_action1 = self._PickupAndRejectPatch(self.cl1_patch1)
    _, reject_action2 = self._PickupAndRejectPatch(self.cl1_patch1)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action1, reject_action2]},
                     self.cl_action_stats.GetTrueRejections())

  def testTrueRejectionsByCQ(self):
    """A complex test filtering for rejections by the cq.

    For a patch that has been rejected by both the pre-cq and cq, only cq's
    actions should be reported. For a patch that has been rejected by only the
    pre-cq, the rejection should not be included at all.
    """
    _, reject_action1 = self._PickupAndRejectPatch(
        self.cl1_patch1, build_config=self.PRE_CQ_BUILD_CONFIG)
    _, reject_action2 = self._PickupAndRejectPatch(
        self.cl1_patch1, build_config=self.CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_SUBMITTED)
    _, reject_action3 = self._PickupAndRejectPatch(
        self.cl2_patch1, build_config=self.PRE_CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl2_patch2, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action1, reject_action2],
                      self.cl2_patch1: [reject_action3]},
                     self.cl_action_stats.GetTrueRejections())
    self.assertEqual({self.cl1_patch1: [reject_action2]},
                     self.cl_action_stats.GetTrueRejections(constants.CQ))

  def testFalseRejectionsSkipApplyFailure(self):
    """Test that apply failures are not considered false rejections."""
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_KICKED_OUT)
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({}, self.cl_action_stats.GetTrueRejections())

  def testFalseRejectionMultiplePatchesFalselyRejected(self):
    """Test the case when we reject mulitple patches falsely."""
    _, reject_action1 = self._PickupAndRejectPatch(self.cl1_patch1)
    _, reject_action2 = self._PickupAndRejectPatch(self.cl1_patch1)
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_SUBMITTED)
    _, reject_action3 = self._PickupAndRejectPatch(self.cl1_patch2)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action1, reject_action2],
                      self.cl1_patch2: [reject_action3]},
                     self.cl_action_stats.GetFalseRejections())

  def testFalseRejectionsByCQ(self):
    """Test that we list CQ spciefic rejections correctly."""
    self._PickupAndRejectPatch(self.cl1_patch1,
                               build_config=self.PRE_CQ_BUILD_CONFIG)
    _, reject_action1 = self._PickupAndRejectPatch(
        self.cl1_patch1, build_config=self.CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl1_patch1, action=constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action1]},
                     self.cl_action_stats.GetFalseRejections(constants.CQ))

  def testFalseRejectionsSkipsBadPreCQRun(self):
    """Test that we don't consider rejections on bad pre-cq buuilds false.

    We batch related CLs together on pre-cq runs. Rejections beause a certain
    pre-cq build failed are considered to not be false because a CL was still to
    blame.
    """
    # Use our own build_ids to tie CLs together.
    bad_build_id = 21
    # This false rejection is due to a bad build.
    self._PickupAndRejectPatch(self.cl1_patch1,
                               build_config=self.PRE_CQ_BUILD_CONFIG,
                               build_id=bad_build_id)
    # This is a true rejection, marking the pre-cq build as a bad build.
    _, reject_action1 = self._PickupAndRejectPatch(
        self.cl2_patch1,
        build_config=self.PRE_CQ_BUILD_CONFIG,
        build_id=bad_build_id)
    self._AppendToHistory(self.cl1_patch1,
                          constants.CL_ACTION_SUBMITTED)
    self._AppendToHistory(self.cl2_patch2,
                          constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl2_patch1: [reject_action1]},
                     self.cl_action_stats.GetTrueRejections())
    self.assertEqual({}, self.cl_action_stats.GetFalseRejections())

  def testFalseRejectionsSkipsBadPreCQAction(self):
    """Test that we skip only the bad pre-cq actions when skipping bad builds.

    If a patch is rejected by a bad pre-cq run, and then rejected again by
    other builds, we should only skip the first action.
    """
    # Use our own build_ids to tie CLs together.
    bad_build_id = 21
    # This false rejection is due to a bad build.
    self._PickupAndRejectPatch(self.cl1_patch1,
                               build_config=self.PRE_CQ_BUILD_CONFIG,
                               build_id=bad_build_id)
    # This is a true rejection, marking the pre-cq build as a bad build.
    _, reject_action1 = self._PickupAndRejectPatch(
        self.cl2_patch1,
        build_config=self.PRE_CQ_BUILD_CONFIG,
        build_id=bad_build_id)
    # This is a valid false rejection.
    _, reject_action2 = self._PickupAndRejectPatch(
        self.cl1_patch1, build_config=self.PRE_CQ_BUILD_CONFIG)
    # This is also a valid false rejection.
    _, reject_action3 = self._PickupAndRejectPatch(
        self.cl1_patch1, build_config=self.CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl1_patch1,
                          constants.CL_ACTION_SUBMITTED)
    self._AppendToHistory(self.cl2_patch2,
                          constants.CL_ACTION_SUBMITTED)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl2_patch1: [reject_action1]},
                     self.cl_action_stats.GetTrueRejections())
    self.assertEqual({self.cl1_patch1: [reject_action2, reject_action3]},
                     self.cl_action_stats.GetFalseRejections())
    self.assertEqual({self.cl1_patch1: [reject_action2]},
                     self.cl_action_stats.GetFalseRejections(constants.PRE_CQ))
    self.assertEqual({self.cl1_patch1: [reject_action3]},
                     self.cl_action_stats.GetFalseRejections(constants.CQ))

  def testFalseRejectionsMergeConflictByBotType(self):
    """Test the case when one bot has merge conflict.

    If pre-cq falsely rejects a patch, and CQ has a merge conflict, but later
    submits the CL, the false rejection should only show up for pre-cq.
    """
    _, reject_action1 = self._PickupAndRejectPatch(
        self.cl1_patch1,
        build_config=self.PRE_CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_KICKED_OUT,
                          build_config=self.CQ_BUILD_CONFIG)
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_SUBMITTED,
                          build_config=self.CQ_BUILD_CONFIG)
    self._CreateCLActionHistory()
    self.assertEqual({self.cl1_patch1: [reject_action1]},
                     self.cl_action_stats.GetFalseRejections(constants.PRE_CQ))
    self.assertEqual({}, self.cl_action_stats.GetFalseRejections(constants.CQ))

  def testRejectionsPatchSubmittedThenUpdated(self):
    """Test the case when a patch is submitted, then updated."""
    _, reject_action1 = self._PickupAndRejectPatch(self.cl1_patch1)
    self._AppendToHistory(self.cl1_patch1, constants.CL_ACTION_SUBMITTED)
    self._AppendToHistory(self.cl1_patch2, constants.CL_ACTION_PICKED_UP)
    self._CreateCLActionHistory()
    self.assertEqual({}, self.cl_action_stats.GetTrueRejections())
    self.assertEqual({self.cl1_patch1: [reject_action1]},
                     self.cl_action_stats.GetFalseRejections())


class TestGerritChangeTuple(cros_test_lib.TestCase):
  """Tests of basic GerritChangeTuple functionality."""

  def testUnknownHostRaises(self):
    with self.assertRaises(clactions.UnknownGerritHostError):
      clactions.GerritChangeTuple.FromHostAndNumber('foobar-host', 1234)

  def testKnownHosts(self):
    self.assertEqual((31415, True),
                     clactions.GerritChangeTuple.FromHostAndNumber(
                         'gerrit-int.chromium.org', 31415))
    self.assertEqual((31415, True),
                     clactions.GerritChangeTuple.FromHostAndNumber(
                         constants.INTERNAL_GERRIT_HOST, 31415))
    self.assertEqual((31415, False),
                     clactions.GerritChangeTuple.FromHostAndNumber(
                         'gerrit.chromium.org', 31415))
    self.assertEqual((31415, False),
                     clactions.GerritChangeTuple.FromHostAndNumber(
                         constants.EXTERNAL_GERRIT_HOST, 31415))

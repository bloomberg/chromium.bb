# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing unit tests for build_failure_message."""

from __future__ import print_function


from chromite.lib import build_failure_message
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import failure_message_lib
from chromite.lib import failure_message_lib_unittest
from chromite.lib import patch_unittest


failure_message_helper = failure_message_lib_unittest.FailureMessageHelper()


class BuildFailureMessageTests(cros_test_lib.MockTestCase):
  """Tests for BuildFailureMessage."""

  def ConstructBuildFailureMessage(self, message_summary='message_summary',
                                   failure_messages=None, internal=True,
                                   reason='reason', builder='builder'):
    return build_failure_message.BuildFailureMessage(
        message_summary, failure_messages, internal, reason, builder)

  def setUp(self):
    self._patch_factory = patch_unittest.MockPatchFactory()

  def _GetBuildFailureMessageWithMixedMsgs(self):
    failure_messages = (
        failure_message_helper.GetBuildFailureMessageWithMixedMsgs())
    build_failure = self.ConstructBuildFailureMessage(
        failure_messages=failure_messages)

    return build_failure

  def testBuildFailureMessageToStr(self):
    """Test BuildFailureMessageToStr."""
    build_failure = self._GetBuildFailureMessageWithMixedMsgs()

    self.assertIsNotNone(build_failure.BuildFailureMessageToStr())

  def testMatchesExceptionCategoriesOnMixedFailuresReturnsFalse(self):
    """Test MatchesExceptionCategories on mixed failures returns False."""
    build_failure = self._GetBuildFailureMessageWithMixedMsgs()

    self.assertFalse(build_failure.MatchesExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))

  def testMatchesExceptionCategoriesOnBuildFailuresReturnsTrue(self):
    """Test MatchesExceptionCategories on build failures returns True."""
    failure_messages = [failure_message_helper.GetBuildScriptFailureMessage(),
                        failure_message_helper.GetPackageBuildFailureMessage()]
    build_failure = self.ConstructBuildFailureMessage(
        failure_messages=failure_messages)

    self.assertTrue(build_failure.MatchesExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))

  def testMatchesExceptionCategoriesOnCompoundFailuresReturnsTrue(self):
    """Test MatchesExceptionCategories on CompoundFailures returns True."""
    f_1 = failure_message_helper.GetBuildScriptFailureMessage(
        failure_id=1, outer_failure_id=3)
    f_2 = failure_message_helper.GetPackageBuildFailureMessage(
        failure_id=2, outer_failure_id=3)
    f_3 = failure_message_helper.GetStageFailureMessage(failure_id=3)
    f_4 = failure_message_helper.GetBuildScriptFailureMessage(failure_id=4)
    failures = (failure_message_lib.FailureMessageManager.ReconstructMessages(
        [f_1, f_2, f_3, f_4]))
    build_failure = self.ConstructBuildFailureMessage(
        failure_messages=failures)

    self.assertTrue(build_failure.MatchesExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))

  def testMatchesExceptionCategoriesOnCompoundFailuresReturnsFalse(self):
    """Test MatchesExceptionCategories on CompoundFailures returns False."""
    f_1 = failure_message_helper.GetStageFailureMessage(
        failure_id=1, outer_failure_id=3)
    f_2 = failure_message_helper.GetPackageBuildFailureMessage(
        failure_id=2, outer_failure_id=3)
    f_3 = failure_message_helper.GetStageFailureMessage(failure_id=3)
    f_4 = failure_message_helper.GetBuildScriptFailureMessage(failure_id=4)
    failures = (failure_message_lib.FailureMessageManager.ReconstructMessages(
        [f_1, f_2, f_3, f_4]))
    build_failure = self.ConstructBuildFailureMessage(
        failure_messages=failures)

    self.assertFalse(build_failure.MatchesExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))

  def testHasExceptionCategoriesOnMixedFailures(self):
    """Test HasExceptionCategories on mixed failures."""
    build_failure = self._GetBuildFailureMessageWithMixedMsgs()

    self.assertTrue(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))
    self.assertTrue(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_UNKNOWN}))
    self.assertFalse(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_INFRA}))
    self.assertFalse(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_LAB}))

  def tesHasExceptionCategoriesOnCompoundFailures(self):
    """Test HasExceptionCategories on CompoundFailures."""
    f_1 = failure_message_helper.GetBuildScriptFailureMessage(
        failure_id=1, outer_failure_id=3)
    f_2 = failure_message_helper.GetPackageBuildFailureMessage(
        failure_id=2, outer_failure_id=3)
    f_3 = failure_message_helper.GetStageFailureMessage(failure_id=3)
    failures = (failure_message_lib.FailureMessageManager.ReconstructMessages(
        [f_1, f_2, f_3]))
    build_failure = self.ConstructBuildFailureMessage(
        failure_messages=failures)

    self.assertTrue(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_BUILD}))
    self.assertTrue(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_UNKNOWN}))
    self.assertFalse(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_INFRA}))
    self.assertFalse(build_failure.HasExceptionCategories(
        {constants.EXCEPTION_CATEGORY_LAB}))

  def _GetMockChanges(self):
    mock_change_1 = self._patch_factory.MockPatch(
        project='chromiumos/overlays/chromiumos-overlay')
    mock_change_2 = self._patch_factory.MockPatch(
        project='chromiumos/overlays/chromiumos-overlay')
    mock_change_3 = self._patch_factory.MockPatch(
        project='chromiumos/chromite')
    mock_change_4 = self._patch_factory.MockPatch(
        project='chromeos/chromeos-admin')
    return [mock_change_1, mock_change_2, mock_change_3, mock_change_4]

  def _CreateBuildFailure(self):
    f_1 = failure_message_helper.GetPackageBuildFailureMessage(failure_id=1)
    f_2 = failure_message_helper.GetPackageBuildFailureMessage(failure_id=2)
    f_3 = failure_message_helper.GetPackageBuildFailureMessage(failure_id=3)
    failures = (failure_message_lib.FailureMessageManager.ReconstructMessages(
        [f_1, f_2, f_3]))
    return self.ConstructBuildFailureMessage(
        failure_messages=failures)

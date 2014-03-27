#!/usr/bin/python

# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gather_builder_stats."""

import datetime
import itertools
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.scripts import gather_builder_stats
from chromite.buildbot import cbuildbot_metadata
from chromite.buildbot import constants

import mock


class TestCLActionLogic(unittest.TestCase):
  """Ensures that CL action analysis logic is correct."""

  def _getTestBuildData(self):
    """Generate a return test data.

    Returns:
      A list of cbuildbot_metadata.BuildData objects to use as
      test data for CL action summary logic.
    """
    # Mock patches for test data.
    c1p1 = cbuildbot_metadata.GerritPatchTuple(1, 1, False)
    c2p1 = cbuildbot_metadata.GerritPatchTuple(2, 1, True)
    c2p2 = cbuildbot_metadata.GerritPatchTuple(2, 2, True)

    # Mock builder status dictionaries
    passed_status = {'status' : constants.FINAL_STATUS_PASSED}
    failed_status = {'status' : constants.FINAL_STATUS_FAILED}

    t = itertools.count()

    TEST_METADATA = [
      # Build 1 picks up no patches.
      cbuildbot_metadata.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 1,
                            'status' : passed_status}),
      # Build 2 picks up c1p1 and does nothing.
      cbuildbot_metadata.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 2,
                            'status' : failed_status}
          ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()),
      # Build 3 picks up c1p1 and c2p1 and rejects both.
      cbuildbot_metadata.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 3,
                            'status' : failed_status}
          ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c2p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c1p1, constants.CL_ACTION_KICKED_OUT, t.next()
          ).RecordCLAction(c2p1, constants.CL_ACTION_KICKED_OUT, t.next()),
      # Build 4 picks up c1p1 and c2p2 and submits both.
      # So  c1p1 should be detected as a 1-time rejected good patch,
      # and c2p1 should be detected as a possibly bad patch.
      cbuildbot_metadata.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 4,
                            'status' : passed_status}
          ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c2p2, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c1p1, constants.CL_ACTION_SUBMITTED, t.next()
          ).RecordCLAction(c2p2, constants.CL_ACTION_SUBMITTED, t.next())
     ]

    # TEST_METADATA should not be guaranteed to be ordered by build number
    # so shuffle it, but use the same seed each time so that unit test is
    # deterministic.
    random.seed(0)
    random.shuffle(TEST_METADATA)

    # Wrap the test metadata into BuildData objects.
    TEST_BUILDDATA = [cbuildbot_metadata.BuildData('', d.GetDict())
                      for d in TEST_METADATA]

    return TEST_BUILDDATA


  def testCLStatsSummary(self):
    with cros_build_lib.ContextManagerStack() as stack:
      test_builddata = self._getTestBuildData()
      stack.Add(mock.patch.object, gather_builder_stats.StatsManager,
                '_FetchBuildData', return_value=test_builddata)
      cl_stats = gather_builder_stats.CLStats()
      cl_stats.Gather(datetime.date.today())
      summary = cl_stats.Summarize()

      expected = {
          'mean_good_patch_rejections': 0.5,
          'unique_patches': 3,
          'total_cl_actions': 9,
          'good_patch_rejection_breakdown': [(0, 1), (1, 1)],
          'good_patches_rejected': 1,
          'submitted_patches': 2,
          'submit_fails': 0,
          'unique_cls': 2,
          'median_handling_time': -1, # This will be ignored in comparison
          'bad_cl_candidates': [
              cbuildbot_metadata.GerritChangeTuple(gerrit_number=2,
                                                   internal=True)],
          'rejections': 2}
      # Ignore handling time in comparison.
      summary['median_handling_time'] = expected['median_handling_time']
      self.assertEqual(summary, expected)

if __name__ == '__main__':
  cros_test_lib.main()

# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for summarize_build_stats."""

from __future__ import print_function

import datetime
import itertools
import mock
import random

from chromite.lib import clactions
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.scripts import gather_builder_stats
from chromite.scripts import summarize_build_stats
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import constants


REASON_BAD_CL = summarize_build_stats.CLStatsEngine.REASON_BAD_CL
CQ = constants.CQ
PRE_CQ = constants.PRE_CQ


class TestCLActionLogic(cros_test_lib.TestCase):
  """Ensures that CL action analysis logic is correct."""

  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()

  def _getTestBuildData(self, cq):
    """Generate a return test data.

    Args:
      cq: Whether this is a CQ run. If False, this is a Pre-CQ run.

    Returns:
      A list of metadata_lib.BuildData objects to use as
      test data for CL action summary logic.
    """
    # Mock patches for test data.
    c1p1 = metadata_lib.GerritPatchTuple(1, 1, False)
    c2p1 = metadata_lib.GerritPatchTuple(2, 1, True)
    c2p2 = metadata_lib.GerritPatchTuple(2, 2, True)
    c3p1 = metadata_lib.GerritPatchTuple(3, 1, True)
    c3p2 = metadata_lib.GerritPatchTuple(3, 2, True)
    c4p1 = metadata_lib.GerritPatchTuple(4, 1, True)
    c4p2 = metadata_lib.GerritPatchTuple(4, 2, True)

    # Mock builder status dictionaries
    passed_status = {'status': constants.FINAL_STATUS_PASSED}
    failed_status = {'status': constants.FINAL_STATUS_FAILED}

    t = itertools.count()
    bot_config = (constants.CQ_MASTER if cq
                  else constants.PRE_CQ_GROUP_CONFIG)

    # pylint: disable=bad-continuation
    TEST_METADATA = [
      # Build 1 picks up no patches.
      metadata_lib.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 1,
                            'bot-config' : bot_config,
                            'results' : [],
                            'status' : passed_status}),
      # Build 2 picks up c1p1 and does nothing.
      metadata_lib.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 2,
                            'bot-config' : bot_config,
                            'results' : [],
                            'status' : failed_status,
                            'changes': [c1p1._asdict()]}
          ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()),
      # Build 3 picks up c1p1 and c2p1 and rejects both.
      # c3p1 is not included in the run because it fails to apply.
      metadata_lib.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 3,
                            'bot-config' : bot_config,
                            'results' : [],
                            'status' : failed_status,
                            'changes': [c1p1._asdict(),
                                        c2p1._asdict()]}
          ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c2p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c1p1, constants.CL_ACTION_KICKED_OUT, t.next()
          ).RecordCLAction(c2p1, constants.CL_ACTION_KICKED_OUT, t.next()
          ).RecordCLAction(c3p1, constants.CL_ACTION_KICKED_OUT, t.next()),
      # Build 4 picks up c4p1 and does nothing with it.
      # c4p2 isn't picked up because it fails to apply.
      metadata_lib.CBuildbotMetadata(
          ).UpdateWithDict({'build-number' : 3,
                            'bot-config' : bot_config,
                            'results' : [],
                            'status' : failed_status,
                            'changes': [c4p1._asdict()]}
          ).RecordCLAction(c4p1, constants.CL_ACTION_PICKED_UP, t.next()
          ).RecordCLAction(c4p2, constants.CL_ACTION_KICKED_OUT, t.next()),
    ]
    if cq:
      TEST_METADATA += [
        # Build 4 picks up c1p1, c2p2, c3p2, c4p1 and submits the first three.
        # c4p2 is submitted without being tested.
        # So  c1p1 should be detected as a 1-time rejected good patch,
        # and c2p1 should be detected as a possibly bad patch.
        metadata_lib.CBuildbotMetadata(
            ).UpdateWithDict({'build-number' : 4,
                              'bot-config' : bot_config,
                              'results' : [],
                              'status' : passed_status,
                              'changes': [c1p1._asdict(),
                                          c2p2._asdict()]}
            ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c2p2, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c3p2, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c4p1, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c1p1, constants.CL_ACTION_SUBMITTED, t.next()
            ).RecordCLAction(c2p2, constants.CL_ACTION_SUBMITTED, t.next()
            ).RecordCLAction(c3p2, constants.CL_ACTION_SUBMITTED, t.next()
            ).RecordCLAction(c4p2, constants.CL_ACTION_SUBMITTED, t.next()),
      ]
    else:
      TEST_METADATA += [
        metadata_lib.CBuildbotMetadata(
            ).UpdateWithDict({'build-number' : 5,
                              'bot-config' : bot_config,
                              'results' : [],
                              'status' : failed_status,
                              'changes': [c4p1._asdict()]}
            ).RecordCLAction(c4p1, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c4p1, constants.CL_ACTION_KICKED_OUT, t.next()),
        metadata_lib.CBuildbotMetadata(
            ).UpdateWithDict({'build-number' : 6,
                              'bot-config' : bot_config,
                              'results' : [],
                              'status' : failed_status,
                              'changes': [c4p1._asdict()]}
            ).RecordCLAction(c1p1, constants.CL_ACTION_PICKED_UP, t.next()
            ).RecordCLAction(c1p1, constants.CL_ACTION_KICKED_OUT, t.next())
      ]
    # pylint: enable=bad-continuation

    # TEST_METADATA should not be guaranteed to be ordered by build number
    # so shuffle it, but use the same seed each time so that unit test is
    # deterministic.
    random.seed(0)
    random.shuffle(TEST_METADATA)

    for m in TEST_METADATA:
      build_id = self.fake_db.InsertBuild(
          m.GetValue('bot-config'), constants.WATERFALL_INTERNAL,
          m.GetValue('build-number'), m.GetValue('bot-config'),
          'bot-hostname')
      m.UpdateWithDict({'build_id': build_id})
      actions = []
      for action_metadata in m.GetDict()['cl_actions']:
        actions.append(clactions.CLAction.FromMetadataEntry(action_metadata))
      self.fake_db.InsertCLActions(build_id, actions)

    # Wrap the test metadata into BuildData objects.
    TEST_BUILDDATA = [metadata_lib.BuildData('', d.GetDict())
                      for d in TEST_METADATA]

    return TEST_BUILDDATA

  def testCLStatsEngineSummary(self):
    with cros_build_lib.ContextManagerStack() as stack:
      pre_cq_builddata = self._getTestBuildData(cq=False)
      cq_builddata = self._getTestBuildData(cq=True)
      stack.Add(mock.patch.object, gather_builder_stats.StatsManager,
                '_FetchBuildData', side_effect=[cq_builddata, pre_cq_builddata])
      stack.Add(mock.patch.object, gather_builder_stats, 'PrepareCreds')
      stack.Add(mock.patch.object, summarize_build_stats.CLStatsEngine,
                'GatherFailureReasonsFromSpreadSheet')
      cl_stats = summarize_build_stats.CLStatsEngine('foo@bar.com',
                                                     self.fake_db)
      cl_stats.Gather(datetime.date.today(), datetime.date.today())
      cl_stats.reasons = {1: '', 2: '', 3: REASON_BAD_CL, 4: REASON_BAD_CL}
      cl_stats.blames = {1: '', 2: '', 3: 'crosreview.com/1',
                         4: 'crosreview.com/1'}
      summary = cl_stats.Summarize()

      expected = {
          'mean_good_patch_rejections': 0.5,
          'unique_patches': 7,
          'unique_blames_change_count': 0,
          'total_cl_actions': 28,
          'good_patch_rejection_breakdown': [(0, 3), (1, 0), (2, 1)],
          'good_patch_rejection_count': {CQ: 1, PRE_CQ: 1},
          'good_patch_rejections': 2,
          'false_rejection_rate': {CQ: 20., PRE_CQ: 20., 'combined': 100. / 3},
          'submitted_patches': 4,
          'submit_fails': 0,
          'unique_cls': 4,
          'median_handling_time': -1,  # This will be ignored in comparison
          'patch_handling_time': -1,  # This will be ignored in comparison
          'bad_cl_candidates': {
              CQ: [metadata_lib.GerritChangeTuple(gerrit_number=2,
                                                  internal=True)],
              PRE_CQ: [metadata_lib.GerritChangeTuple(gerrit_number=2,
                                                      internal=True),
                       metadata_lib.GerritChangeTuple(gerrit_number=4,
                                                      internal=True)],
          },
          'rejections': 10}
      # Ignore handling times in comparison, since these are not fully
      # reproducible from run to run of the unit test.
      summary['median_handling_time'] = expected['median_handling_time']
      summary['patch_handling_time'] = expected['patch_handling_time']
      self.maxDiff = None
      self.assertEqual(summary, expected)

  def testProcessBlameString(self):
    """Tests that bug and CL links are correctly parsed."""
    blame = ('some words then crbug.com/1234, then other junk and '
             'https://code.google.com/p/chromium/issues/detail?id=4321 '
             'then some stuff and other stuff and b/2345 and also '
             'https://b.corp.google.com/issue?id=5432&query=5432 '
             'and then some crosreview.com/3456 or some '
             'https://chromium-review.googlesource.com/#/c/6543/ and '
             'then crosreview.com/i/9876 followed by '
             'https://chrome-internal-review.googlesource.com/#/c/6789/ '
             'blah https://gutsv3.corp.google.com/#ticket/1234 t/4321')
    expected = ['crbug.com/1234',
                'crbug.com/4321',
                'b/2345',
                'b/5432',
                'crosreview.com/3456',
                'crosreview.com/6543',
                'crosreview.com/i/9876',
                'crosreview.com/i/6789',
                't/1234',
                't/4321']
    self.assertEqual(
        summarize_build_stats.CLStatsEngine.ProcessBlameString(blame),
        expected)

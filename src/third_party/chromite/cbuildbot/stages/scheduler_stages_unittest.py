# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for scheduler stages."""

from __future__ import print_function

import mock

from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import scheduler_stages
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import build_requests
from chromite.lib import cidb
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import fake_cidb
from chromite.lib.buildstore import FakeBuildStore


class ScheduleSalvesStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Unit tests for ScheduleSalvesStage."""

  BOT_ID = 'master-paladin'

  def setUp(self):
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value='server_account')
    self.PatchObject(auth.AuthorizedHttp, '__init__',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SendBuildbucketRequest',
                     return_value=None)
    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.sync_stage = mock.Mock()
    self._Prepare()

  def ConstructStage(self):
    bs = FakeBuildStore()
    return scheduler_stages.ScheduleSlavesStage(self._run, bs, self.sync_stage)

  def testRequestBuild(self):
    config = config_lib.BuildConfig(
        name='child',
        build_affinity=True,
        important=True,
        display_label='cq',
        boards=['board_A'], build_type='paladin')

    stage = self.ConstructStage()
    # pylint: disable=protected-access
    request = stage._CreateRequestBuild('child', config, 0, 'master_bb_0', None)
    self.assertEqual(request.build_config, 'child')
    self.assertEqual(request.master_buildbucket_id, 'master_bb_0')
    self.assertEqual(request.extra_args, ['--buildbot'])

  def testRequestBuildWithSnapshotRev(self):
    config = config_lib.BuildConfig(
        name='child',
        build_affinity=True,
        important=True,
        display_label='cq',
        boards=['board_A'], build_type='paladin')

    stage = self.ConstructStage()
    # Set the annealing snapshot revision to pass to the child builders.
    # pylint: disable=protected-access
    stage._run.options.cbb_snapshot_revision = 'hash1234'
    request = stage._CreateRequestBuild('child', config, 0, 'master_bb_1', None)
    self.assertEqual(request.build_config, 'child')
    self.assertEqual(request.master_buildbucket_id, 'master_bb_1')
    expected_extra_args = ['--buildbot', '--cbb_snapshot_revision', 'hash1234']
    self.assertEqual(request.extra_args, expected_extra_args)

  def testPerformStage(self):
    """Test PerformStage."""
    stage = self.ConstructStage()
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)

    stage.PerformStage()

  def testScheduleImportantSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with important slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map_1 = {
        'slave_external': config_lib.BuildConfig(important=True)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map_1)
    self.assertRaises(
        buildbucket_lib.BuildbucketResponseException,
        stage.ScheduleSlaveBuildsViaBuildbucket,
        important_only=False, dryrun=True)

  def testScheduleUnimportantSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with unimportant slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map = {
        'slave_external': config_lib.BuildConfig(important=False),}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    stage.ScheduleSlaveBuildsViaBuildbucket(important_only=False, dryrun=True)

    scheduled_slaves = self._run.attrs.metadata.GetValue(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES)
    self.assertEqual(len(scheduled_slaves), 0)
    unscheduled_slaves = self._run.attrs.metadata.GetValue(
        constants.METADATA_UNSCHEDULED_SLAVES)
    self.assertEqual(len(unscheduled_slaves), 1)

  def testScheduleSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with mixed slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map = {
        'slave_1': config_lib.BuildConfig(important=False),
        'slave_2': config_lib.BuildConfig(important=True),}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    self.assertRaises(
        buildbucket_lib.BuildbucketResponseException,
        stage.ScheduleSlaveBuildsViaBuildbucket,
        important_only=False, dryrun=True)

  def testScheduleSlaveBuildsSuccess(self):
    """Test ScheduleSlaveBuilds with success."""
    stage = self.ConstructStage()

    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     return_value=('buildbucket_id', None))

    slave_config_map = {
        'slave_1': config_lib.BuildConfig(important=False),
        'slave_2': config_lib.BuildConfig(important=True)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)

    stage.ScheduleSlaveBuildsViaBuildbucket(important_only=False, dryrun=True)

    scheduled_slaves = self._run.attrs.metadata.GetValue(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES)
    self.assertEqual(len(scheduled_slaves), 1)
    experimental_slaves = self._run.attrs.metadata.GetValue(
        constants.METADATA_SCHEDULED_EXPERIMENTAL_SLAVES)
    self.assertEqual(len(experimental_slaves), 1)
    unscheduled_slaves = self._run.attrs.metadata.GetValue(
        constants.METADATA_UNSCHEDULED_SLAVES)
    self.assertEqual(len(unscheduled_slaves), 0)

  def testNoScheduledSlaveBuilds(self):
    """Test no slave builds are scheduled."""
    stage = self.ConstructStage()
    schedule_mock = self.PatchObject(
        scheduler_stages.ScheduleSlavesStage,
        'ScheduleSlaveBuildsViaBuildbucket')
    self.sync_stage.pool.HasPickedUpCLs.return_value = False

    stage.PerformStage()
    self.assertFalse(schedule_mock.called)

  def testScheduleSlaveBuildsWithCLs(self):
    """Test no slave builds are scheduled."""
    stage = self.ConstructStage()
    schedule_mock = self.PatchObject(
        scheduler_stages.ScheduleSlavesStage,
        'ScheduleSlaveBuildsViaBuildbucket')
    self.sync_stage.pool.HasPickedUpCLs.return_value = True

    stage.PerformStage()
    self.assertTrue(schedule_mock.called)

  def testPostSlaveBuildToBuildbucket(self):
    """Test PostSlaveBuildToBuildbucket on builds with a single board."""
    content = {'build':{'id':'bb_id_1', 'created_ts':1}}
    self.PatchObject(buildbucket_lib.BuildbucketClient, 'PutBuildRequest',
                     return_value=content)
    slave_config = config_lib.BuildConfig(
        name='slave',
        build_affinity=True,
        important=True,
        display_label='cq',
        boards=['board_A'], build_type='paladin')

    stage = self.ConstructStage()
    buildbucket_id, created_ts = stage.PostSlaveBuildToBuildbucket(
        'slave', slave_config, 0, 'master_bb_id', dryrun=True)

    self.assertEqual(buildbucket_id, 'bb_id_1')
    self.assertEqual(created_ts, 1)

  # pylint: disable=protected-access
  def testScheduleSlaveBuildsViaBuildbucket(self):
    """Test ScheduleSlaveBuildsViaBuildbucket."""
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=(('bb_id_1', None), ('bb_id_2', None)))
    slave_config_map = {
        'important_external': config_lib.BuildConfig(important=True),
        'experimental_external': config_lib.BuildConfig(important=False),}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)

    stage = self.ConstructStage()
    stage.ScheduleSlaveBuildsViaBuildbucket(important_only=False, dryrun=True)

    results = self.fake_db.GetBuildRequestsForBuildConfigs(
        ['important_external'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].request_build_config, 'important_external')
    self.assertEqual(results[0].request_buildbucket_id, 'bb_id_2')
    self.assertEqual(results[0].request_reason,
                     build_requests.REASON_IMPORTANT_CQ_SLAVE)

    results = self.fake_db.GetBuildRequestsForBuildConfigs(
        ['experimental_external'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].request_build_config, 'experimental_external')
    self.assertEqual(results[0].request_buildbucket_id, 'bb_id_1')
    self.assertEqual(results[0].request_reason,
                     build_requests.REASON_EXPERIMENTAL_CQ_SLAVE)

    scheduled_important_builds = stage._run.attrs.metadata.GetValue(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES)
    self.assertEqual(len(scheduled_important_builds), 1)
    self.assertEqual(scheduled_important_builds[0],
                     ('important_external', 'bb_id_2', None))

    scheduled_experimental_builds = stage._run.attrs.metadata.GetValue(
        constants.METADATA_SCHEDULED_EXPERIMENTAL_SLAVES)
    self.assertEqual(len(scheduled_experimental_builds), 1)
    self.assertEqual(scheduled_experimental_builds[0],
                     ('experimental_external', 'bb_id_1', None))

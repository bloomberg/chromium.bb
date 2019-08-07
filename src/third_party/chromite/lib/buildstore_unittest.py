# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for buildstore library."""

from __future__ import print_function

import mock

from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import buildstore
from chromite.lib import buildbucket_v2
from chromite.lib import failure_message_lib

BuildStore = buildstore.BuildStore


class TestBuildStore(cros_test_lib.MockTestCase):
  """Test buildstore.BuildStore."""
  # pylint: disable=protected-access

  def testIsCIDBClientMissing(self):
    """Tests _IsCIDBClientMissing function."""
    # Test CIDB needed and client missing.
    bs = BuildStore(_read_from_bb=False, _write_to_cidb=True)
    self.assertEqual(bs._IsCIDBClientMissing(), True)
    bs = BuildStore(_read_from_bb=True, _write_to_cidb=True)
    self.assertEqual(bs._IsCIDBClientMissing(), True)
    bs = BuildStore(_read_from_bb=False, _write_to_cidb=False)
    self.assertEqual(bs._IsCIDBClientMissing(), True)
    # Test CIDB is needed and client is up and running.
    bs = BuildStore(_read_from_bb=False, _write_to_cidb=True)
    bs.cidb_conn = object()
    self.assertEqual(bs._IsCIDBClientMissing(), False)
    bs = BuildStore(_read_from_bb=True, _write_to_cidb=True)
    bs.cidb_conn = object()
    self.assertEqual(bs._IsCIDBClientMissing(), False)
    bs = BuildStore(_read_from_bb=False, _write_to_cidb=False)
    bs.cidb_conn = object()
    self.assertEqual(bs._IsCIDBClientMissing(), False)
    # Test CIDB is not needed.
    bs = BuildStore(_read_from_bb=True, _write_to_cidb=False)
    self.assertEqual(bs._IsCIDBClientMissing(), False)

  def testIsBuildbucketClientMissing(self):
    """Tests _IsBuildbucketClientMissing function."""
    # Test Buildbucket needed and client missing.
    bs = BuildStore(_read_from_bb=True, _write_to_bb=True)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    bs = BuildStore(_read_from_bb=True, _write_to_bb=False)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    bs = BuildStore(_read_from_bb=False, _write_to_bb=True)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    # Test Buildbucket is needed and client is up and running.
    bs = BuildStore(_read_from_bb=True, _write_to_bb=True)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    bs = BuildStore(_read_from_bb=False, _write_to_bb=True)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    bs = BuildStore(_read_from_bb=True, _write_to_bb=False)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    # Test Buildbucket is not needed.
    bs = BuildStore(_read_from_bb=False, _write_to_bb=False)
    bs._transitioning_to_bb = False
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    # Test _transitioning_to_bb logic.
    bs = BuildStore(_read_from_bb=False, _write_to_bb=False)
    bs._transitioning_to_bb = False
    bs.bb_client = None
    self.assertFalse(bs._IsBuildbucketClientMissing())
    bs._transitioning_to_bb = True
    self.assertTrue(bs._IsBuildbucketClientMissing())

  def testInitializeClientsWithCIDBSetup(self):
    """Tests InitializeClients with mock CIDB."""

    class DummyCIDBConnection(object):
      """Dummy class representing CIDBConnection."""

    # With CIDB setup, cidb_conn is populated.
    self.PatchObject(cidb.CIDBConnectionFactory, 'IsCIDBSetup',
                     return_value=True)
    mock_cidb = DummyCIDBConnection()
    self.PatchObject(cidb.CIDBConnectionFactory,
                     'GetCIDBConnectionForBuilder',
                     return_value=mock_cidb)
    bs = BuildStore()
    result = bs.InitializeClients()
    self.assertEqual(bs.cidb_conn, mock_cidb)
    self.assertEqual(result, True)

  def testInitializeClientsWithoutCIDBSetup(self):
    """Tests InitializeClients with mock CIDB."""

    self.PatchObject(cidb.CIDBConnectionFactory, 'IsCIDBSetup',
                     return_value=False)
    bs = BuildStore()
    self.assertEqual(bs.InitializeClients(), False)

  def testInitializeClientsWhenCIDBIsNotNeeded(self):
    """Test InitializeClients without CIDB requirement."""
    bs = BuildStore(_read_from_bb=True, _write_to_cidb=False)
    bs.cidb_conn = None
    self.PatchObject(BuildStore, '_IsBuildbucketClientMissing',
                     return_value=False)
    # Does not raise exception.
    self.assertEqual(bs.InitializeClients(), True)

  def testInitializeClientsWithBuildbucketSetup(self):
    """Tests InitializeClients with mock Buildbucket."""
    bs = BuildStore()
    self.PatchObject(bs, '_IsCIDBClientMissing',
                     return_value=False)
    result = bs.InitializeClients()
    self.assertTrue(isinstance(bs.bb_client, buildbucket_v2.BuildbucketV2))
    self.assertEqual(result, True)

  def testInitializeClientsWhenBuildbucketIsNotNeeded(self):
    """Test InitializeClients without Buildbucket requirement."""
    bs = BuildStore(_read_from_bb=False, _write_to_bb=False)
    self.PatchObject(BuildStore, '_IsCIDBClientMissing',
                     return_value=False)
    # Does not raise exception.
    self.assertEqual(bs.InitializeClients(), True)

  def testInsertBuild(self):
    """Tests the redirect for InsertBuild function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    # Test CIDB redirect.
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=False)
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuild',
                     return_value=constants.MOCK_BUILD_ID)
    build_id = bs.InsertBuild(
        'builder_name', 12345,
        'something-paladin', 'bot_hostname', master_build_id='master_id',
        timeout_seconds='timeout')
    bs.cidb_conn.InsertBuild.assert_called_once_with(
        'builder_name', 12345, 'something-paladin', 'bot_hostname',
        'master_id', 'timeout', None, None, None)
    self.assertEqual(build_id, constants.MOCK_BUILD_ID)
    # Test Buildbucket redirect.
    bs = BuildStore(_write_to_cidb=False, _write_to_bb=True)
    bs.bb_client = mock.MagicMock()
    self.PatchObject(buildbucket_v2, 'UpdateSelfCommonBuildProperties')
    build_id = bs.InsertBuild(
        'builder_name', 12345,
        'something-paladin', 'bot_hostname', important=True,
        timeout_seconds='timeout')
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_once_with(
        cidb_id=build_id, critical=True)
    self.assertEqual(build_id, 0)

  def testGetKilledChildBuilds(self):
    """Tests the redirect for GetKilledChildBuilds function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_read_from_bb=False)
    bs._transitioning_to_bb = False
    fake_result = [
        {'message_value': 1234},
        {'message_value': 2341},
        {'message_value': 3412}]
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'GetBuildMessages', return_value=fake_result)
    build_identifier = buildstore.BuildIdentifier(cidb_id=1,
                                                  buildbucket_id=1234)
    # Test for buildbucket_ids.
    result = bs.GetKilledChildBuilds(build_identifier)
    bs.cidb_conn.GetBuildMessages.assert_called_once_with(
        1, message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
        message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION)
    self.assertEqual(result, [1234, 2341, 3412])
    bs = BuildStore(_read_from_bb=True)
    bs.bb_client = mock.MagicMock()
    bs.GetKilledChildBuilds(build_identifier)
    bs.bb_client.GetKilledChildBuilds.assert_called_once_with(1234)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetKilledChildBuilds(build_identifier)

  def testInsertBuildMessage(self):
    """Tests the redirect for InsertBuildMessage function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_write_to_bb=True)
    bs.cidb_conn = mock.MagicMock()
    buildbucket_v2.UpdateSelfCommonBuildProperties = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuildMessage')
    bs.InsertBuildMessage(1234,
                          message_value=[8921795536486453568,
                                         8921795536486453567])
    bs.cidb_conn.InsertBuildMessage.assert_called_with(
        1234, message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
        message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION,
        message_value='8921795536486453567', board=None)
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_once_with(
        killed_child_builds=[8921795536486453568, 8921795536486453567])
    # Test error conditions.
    with self.assertRaises(AssertionError):
      bs.InsertBuildMessage(1234, message_value=8921795536486453568)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.InsertBuildMessage(1234, message_value=[8921795536486453568])

  def testGetBuildHistory(self):
    """Tests the redirect for GetBuildHistory function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    build_config = 'some-paladin'
    num_results = 1234
    bs.cidb_conn = mock.MagicMock()
    bs.GetBuildHistory(build_config, num_results, branch='master')
    bs.cidb_conn.GetBuildHistory.assert_called_once_with(
        build_config, num_results, starting_build_id=None, end_date=None,
        ignore_build_id=None, ending_build_id=None, platform_version=None,
        branch='master', start_date=None)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetBuildHistory(build_config, num_results)

  def testInsertBuildStage(self):
    """Tests the redirect for InsertBuildStage function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuildStage',
                     return_value=constants.MOCK_STAGE_ID)
    build_stage_id = bs.InsertBuildStage(
        constants.MOCK_BUILD_ID, 'stage_name')
    bs.cidb_conn.InsertBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID, 'stage_name', None,
        constants.BUILDER_STATUS_PLANNED)
    self.assertEqual(build_stage_id, constants.MOCK_STAGE_ID)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.InsertBuildStage(constants.MOCK_BUILD_ID, 'stage_name')

  def testGetSlaveStatuses(self):
    """Tests the redirect for GetSlaveStatuses function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_read_from_bb=False)
    fake_statuses = object()
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'GetSlaveStatuses',
                     return_value=fake_statuses)
    result = bs.GetSlaveStatuses(buildstore.BuildIdentifier(cidb_id=1234))
    bs.cidb_conn.GetSlaveStatuses.assert_called_once_with(
        1234, None)
    self.assertEqual(result, fake_statuses)
    bs = BuildStore(_read_from_bb=True)
    fake_statuses = object()
    bs.bb_client = mock.MagicMock()
    self.PatchObject(bs.bb_client, 'GetChildStatuses',
                     return_value=fake_statuses)
    result = bs.GetSlaveStatuses(buildstore.BuildIdentifier(
        cidb_id=1234, buildbucket_id=1234))
    self.assertEqual(result, fake_statuses)
    bs.bb_client.GetChildStatuses.assert_called_once_with(1234)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetSlaveStatuses(1234)

  def testStartBuildStage(self):
    """Tests the redirect for StartBuildStage function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    stage_id = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'StartBuildStage', return_value=stage_id)
    ret = bs.StartBuildStage(constants.MOCK_BUILD_ID)
    bs.cidb_conn.StartBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID)
    self.assertEqual(ret, stage_id)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.StartBuildStage(constants.MOCK_BUILD_ID)

  def testWaitBuildStage(self):
    """Tests the redirect for WaitBuildStage function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    stage_id = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'WaitBuildStage', return_value=stage_id)
    ret = bs.WaitBuildStage(constants.MOCK_BUILD_ID)
    bs.cidb_conn.WaitBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID)
    self.assertEqual(ret, stage_id)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.WaitBuildStage(constants.MOCK_BUILD_ID)

  def testFinishBuildStage(self):
    """Tests the redirect for FinishBuildStage function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    stage_id = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'FinishBuildStage', return_value=stage_id)
    ret = bs.FinishBuildStage(constants.MOCK_BUILD_ID, 'status')
    bs.cidb_conn.FinishBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID, 'status')
    self.assertEqual(ret, stage_id)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.FinishBuildStage(constants.MOCK_BUILD_ID, 'status')

  def testInsertFailure(self):
    """Tests the redirect for InsertFailure function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertFailure')
    bs.InsertFailure(
        constants.MOCK_STAGE_ID, 'error_type', 'SomethingFailedException')
    bs.cidb_conn.InsertFailure.assert_called_once_with(
        constants.MOCK_STAGE_ID, 'error_type', 'SomethingFailedException',
        constants.EXCEPTION_CATEGORY_UNKNOWN, None, None)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.InsertFailure(constants.MOCK_STAGE_ID, 'error_type',
                       'SomethingFailedException')

  def testFinishBuild(self):
    """Tests the redirect for FinishBuild function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    status = mock.Mock()
    summary = mock.Mock()
    metadata_url = mock.Mock()
    strict = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'FinishBuild')
    bs.FinishBuild(constants.MOCK_BUILD_ID, status=status, summary=summary,
                   metadata_url=metadata_url, strict=strict)
    bs.cidb_conn.FinishBuild.assert_called_once_with(
        constants.MOCK_BUILD_ID, status=status, summary=summary,
        metadata_url=metadata_url, strict=strict)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.FinishBuild(constants.MOCK_BUILD_ID, status=status, summary=summary,
                     metadata_url=metadata_url, strict=strict)

  def testFinishChildConfig(self):
    """Tests the redirect for FinishChildConfig function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    child_config = mock.Mock()
    status = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'FinishChildConfig')
    bs.FinishChildConfig(constants.MOCK_BUILD_ID, child_config, status=status)
    bs.cidb_conn.FinishChildConfig.assert_called_once_with(
        constants.MOCK_BUILD_ID, child_config, status=status)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.FinishChildConfig(constants.MOCK_BUILD_ID, child_config, status=status)

  def testInsertBoardPerBuildWithoutMetadata(self):
    """Tests the InsertBoardPerBuild function when metadata isn't available."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=True)
    bs.cidb_conn = mock.MagicMock()
    buildbucket_v2.UpdateSelfCommonBuildProperties = mock.MagicMock()
    build_id = 1234
    board = 'grunt'
    bs.InsertBoardPerBuild(build_id, board)
    bs.cidb_conn.InsertBoardPerBuild.assert_called_once_with(
        build_id, board)
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_once_with(
        board=board)

  def testInsertBoardPerBuildWithMetadata(self):
    """Tests the InsertBoardPerBuild function when metadata is available."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=True)
    bs.cidb_conn = mock.MagicMock()
    buildbucket_v2.UpdateSelfCommonBuildProperties = mock.MagicMock()
    build_id = 1234
    board = 'grunt'
    fake_metadata = {'main-firmware-version': '1.2.3.4',
                     'ec-firmware-version': '2.3.4.5'}
    bs.InsertBoardPerBuild(build_id, board, fake_metadata)
    bs.cidb_conn.UpdateBoardPerBuildMetadata.assert_called_once_with(
        build_id, board, fake_metadata)
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_with(
        board=board,
        main_firmware_version=fake_metadata['main-firmware-version'],
        ec_firmware_version=fake_metadata['ec-firmware-version'])

  def testInsertBoardPerBuildWithoutRequisiteClients(self):
    """Tests the redirect for InsertBoardPerBuild function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=False)
    build_id = 1234
    board = 'grunt'
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=True)
    with self.assertRaises(buildstore.BuildStoreException):
      bs.InsertBoardPerBuild(build_id, board)

  def testUpdateMetadata(self):
    """Tests the redirect for UpdateMetadata function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=False)
    bs.cidb_conn = mock.MagicMock()
    fake_metadata = {}
    self.PatchObject(bs.cidb_conn, 'UpdateMetadata')
    bs.UpdateMetadata(constants.MOCK_BUILD_ID, fake_metadata)
    bs.cidb_conn.UpdateMetadata.assert_called_once_with(
        constants.MOCK_BUILD_ID, fake_metadata)
    bs = BuildStore(_write_to_cidb=False, _write_to_bb=True)
    self.PatchObject(buildbucket_v2, 'UpdateBuildMetadata')
    bs.UpdateMetadata(constants.MOCK_BUILD_ID, fake_metadata)
    buildbucket_v2.UpdateBuildMetadata.assert_called_once_with(fake_metadata)
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.UpdateMetadata(constants.MOCK_BUILD_ID, fake_metadata)

  def testGetBuildsFailures(self):
    """Tests the redirect for GetBuildsFailures function."""
    # pylint: disable=protected-access
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_read_from_bb=False)
    bs._transitioning_to_bb = False
    bs.cidb_conn = mock.MagicMock()
    buildbucket_ids = [1234, 2341]
    # Test for CIDB redirect.
    bs.GetBuildsFailures(buildbucket_ids=buildbucket_ids)
    bs.cidb_conn.GetBuildsFailures.assert_called_once_with(
        buildbucket_ids)
    # Test for empty argument.
    self.assertEqual(bs.GetBuildsFailures([]), [])
    fake_return = [{'stage_name': 'stage_1',
                    'stage_status': 'fail',
                    'buildbucket_id': 1234,
                    'build_config': 'something-paladin',
                    'build_status': 'pass',
                    'important': True}]
    bs = BuildStore(_read_from_bb=True)
    bs.bb_client = mock.MagicMock()
    self.PatchObject(bs.bb_client, 'GetStageFailures', return_value=fake_return)
    fail = bs.GetBuildsFailures(buildbucket_ids=[1234])
    bs.bb_client.GetStageFailures.assert_called_with(1234)
    self.assertTrue(isinstance(fail[0], failure_message_lib.StageFailure))
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetBuildsFailures(buildbucket_ids=buildbucket_ids)

  def testGetBuildsStages(self):
    """Tests the redirect for GetBuildsStages function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_read_from_bb=False)
    bs._transitioning_to_bb = False
    bs.cidb_conn = mock.MagicMock()
    buildbucket_ids = ['bucket 1', 'bucket 2']
    # Test for buildbucket_ids.
    bs.GetBuildsStages(buildbucket_ids=buildbucket_ids)
    bs.cidb_conn.GetBuildsStagesWithBuildbucketIds.assert_called_once_with(
        buildbucket_ids)
    bs = BuildStore(_read_from_bb=True)
    bs.bb_client = mock.MagicMock()
    buildbucket_ids = [1234]
    # Test for buildbucket_ids.
    bs.GetBuildsStages(buildbucket_ids=buildbucket_ids)
    bs.bb_client.GetBuildStages.assert_called_once_with(1234)
    # Test for empty argument.
    self.assertEqual(bs.GetBuildsStages([]), [])
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetBuildsStages(buildbucket_ids=buildbucket_ids)

  def testGetBuildStatuses(self):
    """Tests the redirect for GetBuildStatuses function."""
    init = self.PatchObject(BuildStore, 'InitializeClients',
                            return_value=True)
    bs = BuildStore(_read_from_bb=False)
    bs._transitioning_to_bb = False
    bs.cidb_conn = mock.MagicMock()
    build_ids = ['build 1', 'build 2']
    buildbucket_ids = [1234, 2341]
    # Test for build_ids.
    bs.GetBuildStatuses(build_ids=build_ids)
    bs.cidb_conn.GetBuildStatuses.assert_called_once_with(build_ids)
    # Test for buildbucket_ids.
    bs.GetBuildStatuses(buildbucket_ids)
    bs.cidb_conn.GetBuildStatusesWithBuildbucketIds.assert_called_once_with(
        buildbucket_ids)
    bs = BuildStore(_read_from_bb=True)
    bs.bb_client = mock.MagicMock()
    bs.GetBuildStatuses(buildbucket_ids)
    bs.bb_client.GetBuildStatus.assert_called_with(2341)
    # Test for error conditions.
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetBuildStatuses(buildbucket_ids=buildbucket_ids, build_ids=build_ids)
    self.assertEqual(bs.GetBuildStatuses(), [])
    init.return_value = False
    with self.assertRaises(buildstore.BuildStoreException):
      bs.GetBuildStatuses(build_ids=build_ids)

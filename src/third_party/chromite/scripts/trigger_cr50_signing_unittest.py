# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for trigger_cr50_signing.py."""

from __future__ import print_function

import json
import sys

import mock

from chromite.scripts import trigger_cr50_signing as trigger
from chromite.api.gen.chromiumos import common_pb2
from chromite.api.gen.chromiumos import sign_image_pb2
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# pylint: disable=protected-access

class TestLaunchOne(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for the LaunchOne function."""

  def setUp(self):
    self.log_info = self.PatchObject(logging, 'info')
    self.properties = {'keyset': 'test-keyest'}
    self.json_prop = json.dumps(self.properties)

  def testDryRunOnlyLogs(self):
    """Test that dry_run=True results in only a log message."""
    trigger.LaunchOne(True, 'chromeos/packaging/test', self.properties)
    self.assertEqual(0, self.rc.call_count)
    self.log_info.assert_called_once()

  def testCallsRun(self):
    """Test that dry_run=False calls run()."""
    trigger.LaunchOne(False, 'chromeos/packaging/test', self.properties)
    self.log_info.assert_not_called()
    self.assertEqual(
        [mock.call(
            ['bb', 'add', '-p', '@/dev/stdin', 'chromeos/packaging/test'],
            input=self.json_prop, log_output=True)],
        self.rc.call_args_list)


class TestMain(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for the main function."""

  def setUp(self):
    self.log_error = self.PatchObject(logging, 'error')

  def testMinimal(self):
    """Test minimal instructions."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset']
    self.assertEqual(0, trigger.main(args))
    launch.assert_called_once_with(
        False, trigger.CR50_PRODUCTION_JOB, {
            'archive': 'gs://test/file.bin',
            'build_target': {'name': 'unknown'},
            'channel': common_pb2.CHANNEL_UNSPECIFIED,
            'cr50_instructions': {
                'target': sign_image_pb2.Cr50Instructions.PREPVT},
            'image_type': common_pb2.CR50_FIRMWARE,
            'keyset': 'test-keyset',
            'signer_type': sign_image_pb2.SIGNER_PRODUCTION})

  def testPropertiesCorrect(self):
    """Test minimal instructions."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    archive = 'gs://test/file.bin'
    keyset = 'keyset'
    channel = 'canary'
    build_target = 'board'
    target = 'prepvt'
    signer = 'production'
    image = 'cr50_firmware'

    args = ['--archive', archive, '--keyset', keyset, '--channel', channel,
            '--build-target', build_target, '--target', target,
            '--signer-type', signer, '--image-type', image]
    self.assertEqual(0, trigger.main(args))
    launch.assert_called_once_with(
        False, trigger.CR50_PRODUCTION_JOB, {
            'archive': archive,
            'build_target': {'name': build_target},
            'channel': trigger._channels[channel],
            'cr50_instructions': {'target': trigger._target_types[target]},
            'image_type': trigger._image_types[image],
            'keyset': keyset,
            'signer_type': trigger._signer_types[signer]})

  def testStaging(self):
    """Test --staging works."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--staging']
    self.assertEqual(0, trigger.main(args))
    launch.assert_called_once_with(
        False, trigger.CR50_STAGING_JOB, {
            'archive': 'gs://test/file.bin',
            'build_target': {'name': 'unknown'},
            'channel': common_pb2.CHANNEL_UNSPECIFIED,
            'cr50_instructions': {
                'target': sign_image_pb2.Cr50Instructions.PREPVT},
            'image_type': common_pb2.CR50_FIRMWARE,
            'keyset': 'test-keyset',
            'signer_type': sign_image_pb2.SIGNER_PRODUCTION})

  def testDryRun(self):
    """Test --dry-run works."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--dry-run']
    self.assertEqual(0, trigger.main(args))
    launch.assert_called_once_with(
        True, trigger.CR50_PRODUCTION_JOB, {
            'archive': 'gs://test/file.bin',
            'build_target': {'name': 'unknown'},
            'channel': common_pb2.CHANNEL_UNSPECIFIED,
            'cr50_instructions': {
                'target': sign_image_pb2.Cr50Instructions.PREPVT},
            'image_type': common_pb2.CR50_FIRMWARE,
            'keyset': 'test-keyset',
            'signer_type': sign_image_pb2.SIGNER_PRODUCTION})

  def testNodeLockedCatchesBadDeviceId(self):
    """Test --target node_locked catches bad --device-id."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--target', 'node_locked', '--device-id', '12345678-9ABCDEFG',
            '--dev01', '-1', '0x1234']
    self.assertEqual(1, trigger.main(args))
    launch.assert_not_called()
    self.assertEqual(1, self.log_error.call_count)

  def testNodeLockedRequiresDeviceId(self):
    """Test --target node_locked requires --device-id."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--target', 'node_locked']
    self.assertEqual(1, trigger.main(args))
    launch.assert_not_called()
    self.assertEqual(1, self.log_error.call_count)

  def testDeviceIdRequiresNodeLocked(self):
    """Test --device_id is rejected if not node_locked."""
    launch = self.PatchObject(trigger, 'LaunchOne')
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--target', 'general_release', '--dev01', '1', '0x1234']
    self.assertEqual(1, trigger.main(args))
    launch.assert_not_called()
    self.assertEqual(1, self.log_error.call_count)

  def testNodeLockedLaunchesMultiple(self):
    """Test --target node_locked launches multiple jobs."""
    # Do not mock LaunchOne, so that we can grab the input= passed to run().
    args = ['--archive', 'gs://test/file.bin', '--keyset', 'test-keyset',
            '--target', 'node_locked', '--dev01', '1', '0x1234',
            '--dev01', '2', '33']
    self.assertEqual(0, trigger.main(args))
    self.log_error.assert_not_called()
    expected_properties = [
        {'archive': 'gs://test/file.bin', 'build_target': {'name': 'unknown'},
         'channel': 0,
         'cr50_instructions': {'target': trigger._target_types['node_locked'],
                               'device_id': '00000001-00001234'},
         'signer_type': sign_image_pb2.SIGNER_PRODUCTION,
         'image_type': common_pb2.CR50_FIRMWARE, 'keyset': 'test-keyset'},
        {'archive': 'gs://test/file.bin', 'build_target': {'name': 'unknown'},
         'channel': 0,
         'cr50_instructions': {'target': trigger._target_types['node_locked'],
                               'device_id': '00000002-00000021'},
         'signer_type': sign_image_pb2.SIGNER_PRODUCTION,
         'image_type': common_pb2.CR50_FIRMWARE, 'keyset': 'test-keyset'}]
    # Check the calls in two parts, since we need to convert the json string
    # back to a dict.
    self.assertEqual(self.rc.call_args_list, [
        mock.call(
            ['bb', 'add', '-p', '@/dev/stdin', trigger.CR50_PRODUCTION_JOB],
            log_output=True, input=mock.ANY) for _ in expected_properties])
    self.assertEqual(
        expected_properties,
        [json.loads(x[1]['input']) for x in self.rc.call_args_list])

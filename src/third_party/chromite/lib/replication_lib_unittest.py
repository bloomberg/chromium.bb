# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for replication_lib."""

from __future__ import print_function

import json
import os
import stat

from google.protobuf.field_mask_pb2 import FieldMask
from chromite.api.gen.config.replication_config_pb2 import (
    ReplicationConfig, FileReplicationRule, StringReplacementRule,
    FILE_TYPE_JSON, FILE_TYPE_OTHER, REPLICATION_TYPE_COPY,
    REPLICATION_TYPE_FILTER)

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import replication_lib

D = cros_test_lib.Directory


class ReplicateTest(cros_test_lib.TempDirTestCase):
  """Tests of the Replicate method."""

  def setUp(self):
    file_layout = (D('src',
                     ['build_config.json', 'audio_file', 'firmware.bin']),)
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    self.build_config_path = os.path.join(self.tempdir, 'src',
                                          'build_config.json')
    self.audio_path = os.path.join(self.tempdir, 'src', 'audio_file')
    self.firmware_path = os.path.join(self.tempdir, 'src', 'firmware.bin')

    build_config = {
        'chromeos': {
            'configs': [
                {
                    'name': 'A',
                    'field1': 1,
                    'field2': 2,
                },
                {
                    'name': 'B',
                    'field1': 3,
                    'field2': 4,
                },
            ]
        }
    }

    osutils.WriteFile(self.build_config_path, json.dumps(build_config))
    osutils.WriteFile(self.audio_path, '[Speaker A Settings]')
    osutils.WriteFile(self.firmware_path, bytes(123), mode='wb')

  def testReplicate(self):
    """Test basic Replication functionality.

    - destination_fields used to filter JSON payload.
    - firmware.bin not copied.
    """
    build_config_dst_path = os.path.join(self.tempdir, 'dst',
                                         'build_config.json')
    audio_dst_path = os.path.join(self.tempdir, 'dst', 'audio_file')

    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.build_config_path,
            destination_path=build_config_dst_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['name', 'field2'])),
        FileReplicationRule(
            source_path=self.audio_path,
            destination_path=audio_dst_path,
            file_type=FILE_TYPE_OTHER,
            replication_type=REPLICATION_TYPE_COPY,
        ),
    ])

    replication_lib.Replicate(replication_config)

    expected_file_layout = (
        D('src', ['build_config.json', 'audio_file', 'firmware.bin']),
        D('dst', ['build_config.json', 'audio_file']),
    )

    cros_test_lib.VerifyOnDiskHierarchy(self.tempdir, expected_file_layout)

    build_config_dst = json.loads(osutils.ReadFile(build_config_dst_path))
    expected_build_config_dst = {
        'chromeos': {
            'configs': [
                {
                    'name': 'A',
                    'field2': 2,
                },
                {
                    'name': 'B',
                    'field2': 4,
                },
            ]
        }
    }
    self.assertDictEqual(expected_build_config_dst, build_config_dst)

    self.assertFileContents(audio_dst_path, '[Speaker A Settings]')

  def testReplicateDestinationExists(self):
    """Test existing files are overwritten."""
    file_layout = (D('dst', ['audio_file']),)
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    audio_dst_path = os.path.join(self.tempdir, 'dst', 'audio_file')
    osutils.WriteFile(audio_dst_path, 'Original audio data')

    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.audio_path,
            destination_path=audio_dst_path,
            file_type=FILE_TYPE_OTHER,
            replication_type=REPLICATION_TYPE_COPY,
        ),
    ])

    replication_lib.Replicate(replication_config)

    expected_file_layout = (
        D('src', ['build_config.json', 'audio_file', 'firmware.bin']),
        D('dst', ['audio_file']),
    )

    cros_test_lib.VerifyOnDiskHierarchy(self.tempdir, expected_file_layout)

    self.assertFileContents(audio_dst_path, '[Speaker A Settings]')

  def testReplicateInvalidRules(self):
    """Tests invalid FileReplicationRules cause errors."""
    with self.assertRaisesRegex(
        ValueError,
        'Rule for JSON source a.json must use REPLICATION_TYPE_FILTER'):
      replication_lib.Replicate(
          ReplicationConfig(file_replication_rules=[
              FileReplicationRule(
                  source_path='a.json',
                  destination_path='b.json',
                  file_type=FILE_TYPE_JSON,
                  replication_type=REPLICATION_TYPE_COPY,
              ),
          ]))

    with self.assertRaisesRegex(
        ValueError, 'Rule for source a.bin must use REPLICATION_TYPE_COPY'):
      replication_lib.Replicate(
          ReplicationConfig(file_replication_rules=[
              FileReplicationRule(
                  source_path='a.bin',
                  destination_path='b.bin',
                  file_type=FILE_TYPE_OTHER,
                  replication_type=REPLICATION_TYPE_FILTER,
              ),
          ]))

    with self.assertRaisesRegex(
        ValueError,
        'Rule with REPLICATION_TYPE_COPY cannot use destination_fields'):
      replication_lib.Replicate(
          ReplicationConfig(file_replication_rules=[
              FileReplicationRule(
                  source_path='a.bin',
                  destination_path='b.bin',
                  file_type=FILE_TYPE_OTHER,
                  replication_type=REPLICATION_TYPE_COPY,
                  destination_fields=FieldMask(paths=['c', 'd']),
              ),
          ]))

    with self.assertRaisesRegex(
        ValueError,
        'Rule with REPLICATION_TYPE_FILTER must use destination_fields'):
      replication_lib.Replicate(
          ReplicationConfig(file_replication_rules=[
              FileReplicationRule(
                  source_path='a.json',
                  destination_path='b.json',
                  file_type=FILE_TYPE_JSON,
                  replication_type=REPLICATION_TYPE_FILTER,
              ),
          ]))

  def testReplicateFileMode(self):
    """Tests file mode data is replicated."""
    # Check that the original mode is not 777 and then chmod.
    self.assertNotEqual(stat.S_IMODE(os.stat(self.audio_path).st_mode), 0o777)
    os.chmod(self.audio_path, 0o777)

    audio_dst_path = os.path.join(self.tempdir, 'dst', 'audio_file')

    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.audio_path,
            destination_path=audio_dst_path,
            file_type=FILE_TYPE_OTHER,
            replication_type=REPLICATION_TYPE_COPY,
        ),
    ])

    replication_lib.Replicate(replication_config)

    self.assertEqual(stat.S_IMODE(os.stat(audio_dst_path).st_mode), 0o777)

  def testReplicateNonChromeOSConfig(self):
    """Tests replicating a JSON file that is not a ChromeOS Config payload."""
    src_path = os.path.join(self.tempdir, 'src', 'other.json')
    dst_path = os.path.join(self.tempdir, 'dst', 'other.json')
    osutils.WriteFile(src_path, json.dumps({'a': 1, 'b': 2}))

    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=src_path,
            destination_path=dst_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a'])),
    ])


    with self.assertRaisesRegex(
        NotImplementedError, 'Currently only ChromeOS Configs are supported'):
      replication_lib.Replicate(replication_config)

  def testReplicateStringReplicationRules(self):
    audio_dst_path = os.path.join(self.tempdir, 'dst', 'audio_file')

    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.audio_path,
            destination_path=audio_dst_path,
            file_type=FILE_TYPE_OTHER,
            replication_type=REPLICATION_TYPE_COPY,
            string_replacement_rules=[
                StringReplacementRule(before='A', after='B'),
                StringReplacementRule(
                    before='Settings', after='Settings (Updated)'),
            ]),
    ])

    replication_lib.Replicate(replication_config)

    self.assertFileContents(audio_dst_path, '[Speaker B Settings (Updated)]')

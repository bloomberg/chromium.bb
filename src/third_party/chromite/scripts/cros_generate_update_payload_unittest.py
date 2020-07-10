# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_generate_update_payload."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import partial_mock

from chromite.lib.paygen import partition_lib
from chromite.lib.paygen import paygen_payload_lib

from chromite.scripts import cros_generate_update_payload


class CrOSGenerateUpdatePayloadTest(cros_test_lib.MockTestCase):
  """Test correct arguments passed to delta_generator."""

  def testGenerateUpdatePayload(self):
    """Test correct arguments propagated to delta_generator call."""

    paygen_mock = self.PatchObject(paygen_payload_lib, 'GenerateUpdatePayload')

    cros_generate_update_payload.main([
        '--image', 'foo-image',
        '--src_image', 'foo-src-image',
        '--output', 'foo-output',
        '--check',
        '--private_key', 'foo-private-key',
        '--out_metadata_hash_file', 'foo-metadata-hash',
        '--work_dir', 'foo-work-dir',
    ])

    paygen_mock.assert_called_once_with(
        partial_mock.HasString('foo-image'),
        partial_mock.HasString('foo-output'),
        src_image=partial_mock.HasString('foo-src-image'),
        work_dir=partial_mock.HasString('foo-work-dir'),
        private_key=partial_mock.HasString('foo-private-key'),
        check=True,
        out_metadata_hash_file=partial_mock.HasString('foo-metadata-hash'))

  def testExtractPartitions(self):
    """Test extracting partitions."""

    kernel_mock = self.PatchObject(partition_lib, 'ExtractKernel')
    root_mock = self.PatchObject(partition_lib, 'ExtractRoot')

    cros_generate_update_payload.main([
        '--image', 'foo-image',
        '--extract',
        '--kern_path', 'foo-kernel',
        '--root_path', 'foo-root',
        '--root_pretruncate_path', 'foo-pretruncate',
    ])

    kernel_mock.assert_called_once_with(partial_mock.HasString('foo-image'),
                                        partial_mock.HasString('foo-kernel'))
    root_mock.assert_calls_with(partial_mock.HasString('foo-image'),
                                partial_mock.HasString('foo-root'))
    root_mock.assert_calls_with(partial_mock.HasString('foo-image'),
                                partial_mock.HasString('foo-pretruncate'),
                                truncate=False)

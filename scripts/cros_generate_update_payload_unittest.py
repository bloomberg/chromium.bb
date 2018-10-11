# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_generate_update_payload."""

from __future__ import print_function

import os
import tempfile

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib.paygen import partition_lib
from chromite.scripts import cros_generate_update_payload

class DeltaGeneratorTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test correct arguments passed to delta_generator."""
  def testDeltaGenerator(self):
    """Test correct arguments propagated to delta_generator call."""
    temp = os.path.join(self.tempdir, 'temp.bin')
    osutils.WriteFile(temp, '0123456789')
    self.PatchObject(partition_lib, 'ExtractPartition', return_value=temp)
    self.PatchObject(partition_lib, 'Ext2FileSystemSize', return_value=4)

    fake_partitions = (
        cros_build_lib.PartitionInfo(3, 0, 4, 4, 'fs', 'ROOT-A', ''),
    )
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=fake_partitions)
    self.PatchObject(tempfile, 'NamedTemporaryFile',
                     return_value=type('file', (object,), {'name': temp}))
    cros_generate_update_payload.main([
        '--image', '/dev/null',
        '--src_image', '/dev/null',
        '--output', '/dev/null',
    ])

    self.assertCommandContains([
        '--major_version=2',
        '--partition_names=root:kernel',
        '--new_partitions=' + ':'.join([temp, temp]),
        '--new_postinstall_config_file=' + temp,
        '--old_partitions=' + ':'.join([temp, temp]),
        '--rootfs_partition_size=4',
    ])

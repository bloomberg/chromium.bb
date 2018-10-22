# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_generate_update_payload."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cros_generate_update_payload


class CopyFileSegmentTest(cros_test_lib.TempDirTestCase):
  """Test cros_generate_update_payload.CopyFileSegment"""
  def testCopyFileSegment(self):
    """Test copying on a simple file."""
    a = os.path.join(self.tempdir, 'a.txt')
    osutils.WriteFile(a, '789')
    b = os.path.join(self.tempdir, 'b.txt')
    osutils.WriteFile(b, '0123')
    cros_generate_update_payload.CopyFileSegment(a, 'r', 2, b, 'r+', in_seek=1)
    self.assertEqual(osutils.ReadFile(b), '8923')


class ExtractRootTest(cros_test_lib.MockTempDirTestCase):
  """Test cros_generate_update_payload.ExtractRoot"""
  def testTruncate(self):
    """Test truncating on extraction."""
    root = os.path.join(self.tempdir, 'root.bin')
    root_pretruncate = os.path.join(self.tempdir, 'root_pretruncate.bin')

    content = '0123456789'
    osutils.WriteFile(root, content)
    self.PatchObject(cros_generate_update_payload, 'ExtractPartitionToTempFile',
                     return_value=root)
    self.PatchObject(cros_generate_update_payload, 'Ext2FileSystemSize',
                     return_value=2)

    cros_generate_update_payload.ExtractRoot(None, root, root_pretruncate)
    self.assertEqual(osutils.ReadFile(root), content[:2])
    self.assertEqual(osutils.ReadFile(root_pretruncate), content)


class Ext2FileSystemSizeTest(cros_test_lib.MockTestCase):
  """Test cros_generate_update_payload.Ext2FileSystemSize"""
  def testExt2FileSystemSize(self):
    """Test on simple output."""
    block_size = 4096
    block_count = 123

    self.PatchObject(cros_build_lib, 'RunCommand',
                     return_value=cros_build_lib.CommandResult(output='''
Block size: %d
Other thing: 123456798
Not an integer: cdsad132csda
Block count: %d
''' % (block_size, block_count)))

    size = cros_generate_update_payload.Ext2FileSystemSize('/dev/null')
    self.assertEqual(size, block_size * block_count)


class ExtractPartitionToTempFileTest(cros_test_lib.MockTempDirTestCase):
  """Test cros_generate_update_payload.ExtractPartitionToTempFile"""
  def testExtractPartitionToTempFile(self):
    """Tests extraction on a simple image."""
    part_a_bin = '0123'
    part_b_bin = '4567'
    image_bin = part_a_bin + part_b_bin

    image = os.path.join(self.tempdir, 'image.bin')
    osutils.WriteFile(image, image_bin)
    part_a = os.path.join(self.tempdir, 'a.bin')

    fake_partitions = {
        'PART-A': cros_build_lib.PartitionInfo(1, 0, 4, 4, 'fs', 'PART-A', ''),
        'PART-B': cros_build_lib.PartitionInfo(2, 4, 8, 4, 'fs', 'PART-B', ''),
    }
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=fake_partitions)

    cros_generate_update_payload.ExtractPartitionToTempFile(image, 'PART-A',
                                                            temp_file=part_a)
    self.assertEqual(osutils.ReadFile(part_a), part_a_bin)

    # Make sure we correctly generate new temp files.
    tmp = cros_generate_update_payload.ExtractPartitionToTempFile(image,
                                                                  'PART-B')
    self.assertEqual(osutils.ReadFile(tmp), part_b_bin)


class DeltaGeneratorTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test correct arguments passed to delta_generator."""
  def testDeltaGenerator(self):
    """Test correct arguments propagated to delta_generator call."""
    temp = os.path.join(self.tempdir, 'temp.bin')
    osutils.WriteFile(temp, '0123456789')
    self.PatchObject(cros_generate_update_payload, 'ExtractPartitionToTempFile',
                     return_value=temp)
    self.PatchObject(cros_generate_update_payload, 'Ext2FileSystemSize',
                     return_value=4)

    fake_partitions = {
        'ROOT-A': cros_build_lib.PartitionInfo(3, 0, 4, 4, 'fs', 'ROOT-A', ''),
    }
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=fake_partitions)
    cros_generate_update_payload.main([
        '--image', '/dev/null',
        '--src_image', '/dev/null',
        '--output', '/dev/null',
    ])

    self.assertCommandContains([
        '--major_version=1',
        '--new_image=' + temp,
        '--new_kernel=' + temp,
        '--old_image=' + temp,
        '--old_kernel=' + temp,
        '--rootfs_partition_size=4',
    ])

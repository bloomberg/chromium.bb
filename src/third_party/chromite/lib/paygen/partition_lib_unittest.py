# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the partition_lib module."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import path_util

from chromite.lib.paygen import partition_lib

class PartitionLibTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test partition_lib functions."""

  def testTruncate(self):
    """Test truncating on extraction."""
    root = os.path.join(self.tempdir, 'root.bin')

    content = '0123456789'
    osutils.WriteFile(root, content)
    self.PatchObject(partition_lib, 'ExtractPartition')
    self.PatchObject(partition_lib, 'Ext2FileSystemSize', return_value=2)

    partition_lib.ExtractRoot(None, root, truncate=False)
    self.assertEqual(osutils.ReadFile(root), content)

    partition_lib.ExtractRoot(None, root)
    self.assertEqual(osutils.ReadFile(root), content[:2])

  def testExt2FileSystemSize(self):
    """Test getting filesystem size on a simple output."""
    block_size = 4096
    block_count = 123

    self.PatchObject(cros_build_lib, 'RunCommand',
                     return_value=cros_build_lib.CommandResult(output='''
Block size: %d
Other thing: 123456798
Not an integer: cdsad132csda
Block count: %d
''' % (block_size, block_count)))

    size = partition_lib.Ext2FileSystemSize('/dev/null')
    self.assertEqual(size, block_size * block_count)

  def testExtractPartition(self):
    """Tests extraction on a simple image."""
    part_a_bin = '0123'
    part_b_bin = '4567'
    image_bin = part_a_bin + part_b_bin

    image = os.path.join(self.tempdir, 'image.bin')
    osutils.WriteFile(image, image_bin)
    part_a = os.path.join(self.tempdir, 'a.bin')

    fake_partitions = (
        cros_build_lib.PartitionInfo(1, 0, 4, 4, 'fs', 'PART-A', ''),
        cros_build_lib.PartitionInfo(2, 4, 8, 4, 'fs', 'PART-B', ''),
    )
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=fake_partitions)

    partition_lib.ExtractPartition(image, 'PART-A', part_a)
    self.assertEqual(osutils.ReadFile(part_a), part_a_bin)

  def testIsSquashfsImage(self):
    """Tests we correctly identify a SquashFS image."""
    # Tests correct arguments are passed and SquashFS image is correctly
    # identified.
    # Don't need to test the path util functionality, make it just give us the
    # image path back.
    self.PatchObject(path_util, 'ToChrootPath', side_effect=lambda x: x)
    image = '/foo/image'
    self.assertTrue(partition_lib.IsSquashfsImage(image))
    self.assertCommandCalled(['unsquashfs', '-s', image], enter_chroot=True,
                             redirect_stdout=True)

    # Tests failure to identify.
    self.PatchObject(cros_build_lib, 'RunCommand',
                     side_effect=cros_build_lib.RunCommandError("error", 1))
    self.assertFalse(partition_lib.IsSquashfsImage(image))

  def testIsExt4Image(self):
    """Tests we correctly identify an Ext4 image."""
    # Tests correct arguments are passed and Ext4 image is correctly identified.
    # Don't need to test the path util functionality, make it just give us the
    # image path back.
    self.PatchObject(path_util, 'ToChrootPath', side_effect=lambda x: x)
    image = '/foo/image'
    self.assertTrue(partition_lib.IsExt4Image(image))
    self.assertCommandCalled(['sudo', '--', 'tune2fs', '-l', image],
                             enter_chroot=True,
                             redirect_stdout=True)

    # Tests failure to identify.
    self.PatchObject(cros_build_lib, 'RunCommand',
                     side_effect=cros_build_lib.RunCommandError("error", 1))
    self.assertFalse(partition_lib.IsExt4Image(image))

  def testIsGptImage(self):
    """Tests we correctly identify an Gpt image."""
    # Tests correct arguments are passed and Gpt image is correctly identified.
    image = '/foo/image'
    part_info_mock = self.PatchObject(cros_build_lib,
                                      'GetImageDiskPartitionInfo')
    self.assertTrue(partition_lib.IsGptImage(image))
    part_info_mock.assert_called_once_with(image)

    # Tests failure to identify.
    part_info_mock.side_effect = cros_build_lib.RunCommandError("error", 1)
    part_info_mock.return_value = []
    self.assertFalse(partition_lib.IsGptImage(image))

  def testLookupImageType(self):
    """Tests we correctly identify different image types."""
    image = '/foo/image'
    is_gpt = self.PatchObject(partition_lib, 'IsGptImage')
    is_squashfs = self.PatchObject(partition_lib, 'IsSquashfsImage')
    is_ext4 = self.PatchObject(partition_lib, 'IsExt4Image')

    is_gpt.return_value = True
    self.assertEqual(partition_lib.LookupImageType(image),
                     partition_lib.CROS_IMAGE)

    is_gpt.return_value = False
    is_squashfs.return_value = True
    self.assertEqual(partition_lib.LookupImageType(image),
                     partition_lib.DLC_IMAGE)

    is_squashfs.return_value = False
    is_ext4.return_value = True
    self.assertEqual(partition_lib.LookupImageType(image),
                     partition_lib.DLC_IMAGE)

    is_ext4.return_value = False
    self.assertIsNone(partition_lib.LookupImageType(image))

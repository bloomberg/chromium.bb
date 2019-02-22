# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for build_dlc."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import build_dlc


_SRC_DIR = '/tmp/src/'
_FS_TYPE_SQUASHFS = 'squashfs'
_FS_TYPE_EXT4 = 'ext4'
_PRE_ALLOCATED_BLOCKS = 100
_VERSION = '1.0'
_ID = 'id'
_NAME = 'name'


def GetDLCGenerator(temp_dir, fs_type):
  """Factory method for a DLCGenerator object"""
  return build_dlc.DLCGenerator(temp_dir, _SRC_DIR, fs_type,
                                _PRE_ALLOCATED_BLOCKS, _VERSION, _ID, _NAME)


class HashFileTest(cros_test_lib.TempDirTestCase):
  """Test build_dlc.HashFile"""
  def testHashFile(self):
    """Test the hash of a simple file."""
    file_path = os.path.join(self.tempdir, 'f.txt')
    osutils.WriteFile(file_path, '0123')
    hash_value = build_dlc.HashFile(file_path)
    self.assertEqual(hash_value,
                     '1be2e452b46d7a0d9656bbb1f768e824'
                     '8eba1b75baed65f5d99eafa948899a6a')


class SquashOwnershipsTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test build_dlc.SquashOwnershipsTest"""
  def testSquashOwnerships(self):
    GetDLCGenerator(self.tempdir, _FS_TYPE_EXT4).SquashOwnerships(self.tempdir)
    self.assertCommandContains(['chown', '-R', '0:0'])
    self.assertCommandContains(['find'])

class CreateExt4ImageTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test build_dlc.CreateExt4Image"""
  def testCreateExt4Image(self):
    """Test that command is run with correct parameters."""
    GetDLCGenerator(self.tempdir, _FS_TYPE_EXT4).CreateExt4Image()
    self.assertCommandContains(['/sbin/mkfs.ext4', '-b', '4096', '-O',
                                '^has_journal'])
    self.assertCommandContains(['mount', '-o', 'loop,rw'])
    self.assertCommandContains(['cp', '-a', _SRC_DIR])
    self.assertCommandContains(['umount'])
    self.assertCommandContains(['/sbin/e2fsck', '-y', '-f'])
    self.assertCommandContains(['/sbin/resize2fs', '-M'])


class CreateSquashfsImageTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test build_dlc.CreateImageSquashfs"""
  def testCreateSquashfsImage(self):
    """Test that commands are run with correct parameters."""
    GetDLCGenerator(self.tempdir, _FS_TYPE_SQUASHFS).CreateSquashfsImage()
    self.assertCommandContains(['mksquashfs', '-4k-align', '-noappend'])


class GetImageloaderJsonContentTest(cros_test_lib.TestCase):
  """Test build_dlc.GetImageloaderJsonContent"""
  def testGetImageloaderJsonContent(self):
    """Test that GetImageloaderJsonContent returns correct content."""
    blocks = 100
    content = GetDLCGenerator("", _FS_TYPE_SQUASHFS).GetImageloaderJsonContent(
        '01234567', 'deadbeef', blocks)
    self.assertEqual(content, {
        'fs-type': _FS_TYPE_SQUASHFS,
        'pre-allocated-size': _PRE_ALLOCATED_BLOCKS * 4096,
        'id': 'id',
        'size': blocks * 4096,
        'table-sha256-hash': 'deadbeef',
        'name': _NAME,
        'image-sha256-hash': '01234567',
        'image-type': 'dlc',
        'version': _VERSION,
        'is-removable': True,
        'manifest-version': 1,
    })

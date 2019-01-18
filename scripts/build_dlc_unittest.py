# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for build_dlc."""

from __future__ import print_function

import os

import mock

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock

from chromite.scripts import build_dlc


_FS_TYPE_SQUASHFS = 'squashfs'
_FS_TYPE_EXT4 = 'ext4'
_PRE_ALLOCATED_BLOCKS = 100
_VERSION = '1.0'
_ID = 'id'
_NAME = 'name'


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


class DlcGeneratorTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests DlcGenerator."""

  def GetDlcGenerator(self, fs_type=_FS_TYPE_SQUASHFS):
    """Factory method for a DcGenerator object"""
    src_dir = os.path.join(self.tempdir, 'src')
    osutils.SafeMakedirs(src_dir)
    return build_dlc.DlcGenerator(self.tempdir, self.tempdir, src_dir, fs_type,
                                  _PRE_ALLOCATED_BLOCKS, _VERSION, _ID, _NAME)

  def testSquashOwnerships(self):
    """Test build_dlc.SquashOwnershipsTest"""
    self.GetDlcGenerator().SquashOwnerships(self.tempdir)
    self.assertCommandContains(['chown', '-R', '0:0'])
    self.assertCommandContains(['find'])

  def testCreateExt4Image(self):
    """Test CreateExt4Image to make sure it runs with valid parameters."""
    copy_dir_mock = self.PatchObject(osutils, 'CopyDirContents')
    mount_mock = self.PatchObject(osutils, 'MountDir')
    umount_mock = self.PatchObject(osutils, 'UmountDir')

    self.GetDlcGenerator(fs_type=_FS_TYPE_EXT4).CreateExt4Image()
    self.assertCommandContains(['/sbin/mkfs.ext4', '-b', '4096', '-O',
                                '^has_journal'])
    self.assertCommandContains(['/sbin/e2fsck', '-y', '-f'])
    self.assertCommandContains(['/sbin/resize2fs', '-M'])
    copy_dir_mock.assert_called_once_with(partial_mock.HasString('src'),
                                          partial_mock.HasString('root'))
    mount_mock.assert_called_once_with(mock.ANY,
                                       partial_mock.HasString('mount_point'),
                                       mount_opts=('loop', 'rw'))
    umount_mock.assert_called_once_with(partial_mock.HasString('mount_point'))

  def testCreateSquashfsImage(self):
    """Test that creating squashfs commands are run with correct parameters."""
    copy_dir_mock = self.PatchObject(osutils, 'CopyDirContents')

    self.GetDlcGenerator().CreateSquashfsImage()
    self.assertCommandContains(['mksquashfs', '-4k-align', '-noappend'])
    copy_dir_mock.assert_called_once_with(partial_mock.HasString('src'),
                                          partial_mock.HasString('root'))

  def testGetImageloaderJsonContent(self):
    """Test that GetImageloaderJsonContent returns correct content."""
    blocks = 100
    content = self.GetDlcGenerator().GetImageloaderJsonContent(
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

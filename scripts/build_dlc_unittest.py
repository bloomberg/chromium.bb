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
from chromite.scripts import cros_set_lsb_release


_FS_TYPE_SQUASHFS = 'squashfs'
_FS_TYPE_EXT4 = 'ext4'
_PRE_ALLOCATED_BLOCKS = 100
_VERSION = '1.0'
_ID = 'id'
_PACKAGE = 'package'
_NAME = 'name'
_META_DIR = 'opt/google/dlc/'
_IMAGE_DIR = 'build/rootfs/dlc/'

_BLOCK_SIZE = 4096


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

    sysroot = os.path.join(self.tempdir, 'build_root')
    osutils.WriteFile(os.path.join(sysroot, build_dlc.LSB_RELEASE),
                      '%s=%s\n' % (cros_set_lsb_release.LSB_KEY_APPID_RELEASE,
                                   'foo'),
                      makedirs=True)
    ue_conf = os.path.join(sysroot, 'etc', 'update_engine.conf')
    osutils.WriteFile(ue_conf, 'foo-content', makedirs=True)

    return build_dlc.DlcGenerator(src_dir=src_dir,
                                  sysroot=sysroot,
                                  install_root_dir=self.tempdir,
                                  fs_type=fs_type,
                                  pre_allocated_blocks=_PRE_ALLOCATED_BLOCKS,
                                  version=_VERSION,
                                  dlc_id=_ID,
                                  dlc_package=_PACKAGE,
                                  name=_NAME)

  def testSetInstallDir(self):
    """Tests install_root_dir is used correclty."""
    generator = self.GetDlcGenerator()
    self.assertEqual(generator.meta_dir,
                     os.path.join(self.tempdir, _META_DIR, _ID, _PACKAGE))
    self.assertEqual(generator.image_dir,
                     os.path.join(self.tempdir, _IMAGE_DIR, _ID, _PACKAGE))

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
                                          partial_mock.HasString('root'),
                                          symlinks=True)
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
                                          partial_mock.HasString('root'),
                                          symlinks=True)

  def testPrepareLsbRelease(self):
    """Tests that lsb-release is created correctly."""
    generator = self.GetDlcGenerator()
    dlc_dir = os.path.join(self.tempdir, 'dlc_dir')

    generator.PrepareLsbRelease(dlc_dir)

    expected_lsb_release = '\n'.join([
        'DLC_ID=%s' % _ID,
        'DLC_PACKAGE=%s' % _PACKAGE,
        'DLC_NAME=%s' % _NAME,
        'DLC_RELEASE_APPID=foo_%s' % _ID,
    ]) + '\n'

    self.assertEqual(osutils.ReadFile(os.path.join(dlc_dir, 'etc/lsb-release')),
                     expected_lsb_release)

  def testCollectExtraResources(self):
    """Tests that extra resources are collected correctly."""
    generator = self.GetDlcGenerator()

    dlc_dir = os.path.join(self.tempdir, 'dlc_dir')
    generator.CollectExtraResources(dlc_dir)

    ue_conf = 'etc/update_engine.conf'
    self.assertEqual(
        osutils.ReadFile(os.path.join(self.tempdir, 'build_root', ue_conf)),
        'foo-content')

  def testGetImageloaderJsonContent(self):
    """Test that GetImageloaderJsonContent returns correct content."""
    blocks = 100
    content = self.GetDlcGenerator().GetImageloaderJsonContent(
        '01234567', 'deadbeef', blocks)
    self.assertEqual(content, {
        'fs-type': _FS_TYPE_SQUASHFS,
        'pre-allocated-size': _PRE_ALLOCATED_BLOCKS * 4096,
        'id': _ID,
        'package': _PACKAGE,
        'size': blocks * 4096,
        'table-sha256-hash': 'deadbeef',
        'name': _NAME,
        'image-sha256-hash': '01234567',
        'image-type': 'dlc',
        'version': _VERSION,
        'is-removable': True,
        'manifest-version': 1,
    })

  def testVerifyImageSize(self):
    """Test that VerifyImageSize throws exception on errors only."""
    # Succeeds since image size is smaller than preallocated size.
    self.PatchObject(os.path, 'getsize',
                     return_value=(_PRE_ALLOCATED_BLOCKS - 1) * _BLOCK_SIZE)
    self.GetDlcGenerator().VerifyImageSize()

    with self.assertRaises(ValueError):
      # Fails since image size is bigger than preallocated size.
      self.PatchObject(os.path, 'getsize',
                       return_value=(_PRE_ALLOCATED_BLOCKS + 1) * _BLOCK_SIZE)
      self.GetDlcGenerator().VerifyImageSize()

  def testGetOptimalImageBlockSize(self):
    """Test that GetOptimalImageBlockSize returns the valid block size."""
    dlc_generator = self.GetDlcGenerator()
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(0), 0)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(1), 1)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(_BLOCK_SIZE), 1)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(_BLOCK_SIZE+1), 2)


class FinalizeDlcsTest(cros_test_lib.MockTempDirTestCase):
  """Tests functions that generate the final DLC images."""

  def testCopyAllDlcs(self):
    """Tests CopyAllDlcs to make sure all DLCs are copied correctly"""
    # copy_contents_mock = self.PatchObject(osutils, 'CopyDirContents')
    sysroot = os.path.join(self.tempdir, 'sysroot')
    osutils.WriteFile(os.path.join(sysroot, _IMAGE_DIR, _ID, 'dlc.img'),
                      'content', makedirs=True)
    output = os.path.join(self.tempdir, 'output')
    build_dlc.CopyAllDlcs(sysroot, output)
    self.assertExists(os.path.join(output, 'dlc', _ID, 'dlc.img'))

  def testCopyAllDlcsNoDlc(self):
    copy_contents_mock = self.PatchObject(osutils, 'CopyDirContents')
    sysroot = os.path.join(self.tempdir, 'sysroot')
    output = os.path.join(self.tempdir, 'output')
    build_dlc.CopyAllDlcs(sysroot, output)
    copy_contents_mock.assert_not_called()

# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for build_dlc."""

from __future__ import print_function

import json
import os
import sys

import mock

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock

from chromite.scripts import build_dlc
from chromite.scripts import cros_set_lsb_release


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'

_FS_TYPE_SQUASHFS = 'squashfs'
_FS_TYPE_EXT4 = 'ext4'
_PRE_ALLOCATED_BLOCKS = 100
_VERSION = '1.0'
_ID = 'id'
_PACKAGE = 'package'
_NAME = 'name'
_DESCRIPTION = 'description'
_META_DIR = 'opt/google/dlc/'
_IMAGE_DIR = 'build/rootfs/dlc/'
_BOARD = 'test_board'
_FULLNAME_REV = None
_BLOCK_SIZE = 4096

# pylint: disable=protected-access


class UtilsTest(cros_test_lib.TempDirTestCase):
  """Tests build_dlc utility functions."""

  def testHashFile(self):
    """Test the hash of a simple file."""
    file_path = os.path.join(self.tempdir, 'f.txt')
    osutils.WriteFile(file_path, '0123')
    hash_value = build_dlc.HashFile(file_path)
    self.assertEqual(
        hash_value, '1be2e452b46d7a0d9656bbb1f768e824'
        '8eba1b75baed65f5d99eafa948899a6a')

  def testValidateDlcIdentifier(self):
    """Tests build_dlc.ValidateDlcIdentifier."""
    parser = build_dlc.GetParser()
    build_dlc.ValidateDlcIdentifier(parser, 'hello-world')
    build_dlc.ValidateDlcIdentifier(parser, 'hello-world2')
    build_dlc.ValidateDlcIdentifier(parser,
                                    'this-string-has-length-40-exactly-now---')

    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier, '')
    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier, '-')
    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier, '-hi')
    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier, 'hello%')
    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier, 'hello_world')
    self.assertRaises(Exception, build_dlc.ValidateDlcIdentifier,
                      'this-string-has-length-greater-than-40-now')


class EbuildParamsTest(cros_test_lib.TempDirTestCase):
  """Tests EbuildParams functions."""

  def testGetParamsPath(self):
    """Tests EbuildParams.GetParamsPath"""
    install_root_dir = os.path.join(self.tempdir, 'install_root_dir')

    self.assertEqual(
        build_dlc.EbuildParams.GetParamsPath(install_root_dir, _ID, _PACKAGE),
        os.path.join(install_root_dir, build_dlc.DLC_BUILD_DIR, _ID, _PACKAGE,
                     build_dlc.EBUILD_PARAMETERS))

  def CheckParams(self,
                  ebuild_params,
                  dlc_id=_ID,
                  dlc_package=_PACKAGE,
                  fs_type=_FS_TYPE_SQUASHFS,
                  name=_NAME,
                  description=_DESCRIPTION,
                  pre_allocated_blocks=_PRE_ALLOCATED_BLOCKS,
                  version=_VERSION,
                  preload=False,
                  used_by=build_dlc._USED_BY_SYSTEM,
                  fullnamerev=_FULLNAME_REV):
    """Tests EbuildParams JSON values"""
    self.assertDictEqual(ebuild_params,
                         {'dlc_id': dlc_id,
                          'dlc_package': dlc_package,
                          'fs_type': fs_type,
                          'pre_allocated_blocks': pre_allocated_blocks,
                          'version': version,
                          'name': name,
                          'description': description,
                          'preload': preload,
                          'used_by': used_by,
                          'fullnamerev': fullnamerev})

  def GenerateParams(self,
                     install_root_dir,
                     dlc_id=_ID,
                     dlc_package=_PACKAGE,
                     fs_type=_FS_TYPE_SQUASHFS,
                     name=_NAME,
                     description=_DESCRIPTION,
                     pre_allocated_blocks=_PRE_ALLOCATED_BLOCKS,
                     version=_VERSION,
                     preload=False,
                     used_by=build_dlc._USED_BY_SYSTEM,
                     fullnamerev=_FULLNAME_REV):
    """Creates and Stores DLC params at install_root_dir"""
    params = build_dlc.EbuildParams(
        dlc_id=dlc_id,
        dlc_package=dlc_package,
        fs_type=fs_type,
        name=name,
        description=description,
        pre_allocated_blocks=pre_allocated_blocks,
        version=version,
        preload=preload,
        used_by=used_by,
        fullnamerev=fullnamerev)
    return params.StoreDlcParameters(
        install_root_dir=install_root_dir, sudo=False)

  def testStoreDlcParameters(self):
    """Tests EbuildParams.StoreDlcParameters"""
    sysroot = os.path.join(self.tempdir, 'build_root')
    self.GenerateParams(sysroot)
    ebuild_params_path = os.path.join(sysroot, build_dlc.DLC_BUILD_DIR, _ID,
                                      _PACKAGE, build_dlc.EBUILD_PARAMETERS)
    self.assertExists(ebuild_params_path)

    with open(ebuild_params_path, 'rb') as f:
      self.CheckParams(json.load(f))

  def testLoadDlcParameters(self):
    """Tests EbuildParams.LoadDlcParameters"""
    sysroot = os.path.join(self.tempdir, 'build_root')
    self.GenerateParams(sysroot)
    ebuild_params_class = build_dlc.EbuildParams.LoadEbuildParams(
        sysroot, _ID, _PACKAGE)
    self.CheckParams(ebuild_params_class.__dict__)


class DlcGeneratorTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests DlcGenerator."""

  def setUp(self):
    self.ExpectRootOwnedFiles()

  def GetDlcGenerator(self, fs_type=_FS_TYPE_SQUASHFS):
    """Factory method for a DcGenerator object"""
    src_dir = os.path.join(self.tempdir, 'src')
    osutils.SafeMakedirs(src_dir)

    sysroot = os.path.join(self.tempdir, 'build_root')
    osutils.WriteFile(
        os.path.join(sysroot, build_dlc.LSB_RELEASE),
        '%s=%s\n' % (cros_set_lsb_release.LSB_KEY_APPID_RELEASE, 'foo'),
        makedirs=True)
    ue_conf = os.path.join(sysroot, 'etc', 'update_engine.conf')
    osutils.WriteFile(ue_conf, 'foo-content', makedirs=True)

    params = build_dlc.EbuildParams(
        dlc_id=_ID,
        dlc_package=_PACKAGE,
        fs_type=fs_type,
        name=_NAME,
        description=_DESCRIPTION,
        pre_allocated_blocks=_PRE_ALLOCATED_BLOCKS,
        version=_VERSION,
        preload=False,
        used_by=build_dlc._USED_BY_SYSTEM,
        fullnamerev=_FULLNAME_REV)
    return build_dlc.DlcGenerator(
        ebuild_params=params,
        src_dir=src_dir,
        sysroot=sysroot,
        install_root_dir=sysroot,
        board=_BOARD)

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
    self.assertCommandContains(
        ['/sbin/mkfs.ext4', '-b', '4096', '-O', '^has_journal'])
    self.assertCommandContains(['/sbin/e2fsck', '-y', '-f'])
    self.assertCommandContains(['/sbin/resize2fs', '-M'])
    copy_dir_mock.assert_called_once_with(
        partial_mock.HasString('src'),
        partial_mock.HasString('root'),
        symlinks=True)
    mount_mock.assert_called_once_with(
        mock.ANY,
        partial_mock.HasString('mount_point'),
        mount_opts=('loop', 'rw'))
    umount_mock.assert_called_once_with(partial_mock.HasString('mount_point'))

  def testCreateSquashfsImage(self):
    """Test that creating squashfs commands are run with correct parameters."""
    copy_dir_mock = self.PatchObject(osutils, 'CopyDirContents')

    self.GetDlcGenerator().CreateSquashfsImage()
    self.assertCommandContains(['mksquashfs', '-4k-align', '-noappend'])
    copy_dir_mock.assert_called_once_with(
        partial_mock.HasString('src'),
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

    self.assertEqual(
        osutils.ReadFile(os.path.join(dlc_dir, 'etc/lsb-release')),
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
    self.assertEqual(
        content, {
            'fs-type': _FS_TYPE_SQUASHFS,
            'pre-allocated-size': str(_PRE_ALLOCATED_BLOCKS * _BLOCK_SIZE),
            'id': _ID,
            'package': _PACKAGE,
            'size': str(blocks * _BLOCK_SIZE),
            'table-sha256-hash': 'deadbeef',
            'name': _NAME,
            'description': _DESCRIPTION,
            'image-sha256-hash': '01234567',
            'image-type': 'dlc',
            'version': _VERSION,
            'is-removable': True,
            'manifest-version': 1,
            'preload-allowed': False,
            'used-by': build_dlc._USED_BY_SYSTEM,
        })

  def testVerifyImageSize(self):
    """Test that VerifyImageSize throws exception on errors only."""
    # Succeeds since image size is smaller than preallocated size.
    self.PatchObject(
        os.path,
        'getsize',
        return_value=(_PRE_ALLOCATED_BLOCKS - 1) * _BLOCK_SIZE)
    self.GetDlcGenerator().VerifyImageSize()

    with self.assertRaises(ValueError):
      # Fails since image size is bigger than preallocated size.
      self.PatchObject(
          os.path,
          'getsize',
          return_value=(_PRE_ALLOCATED_BLOCKS + 1) * _BLOCK_SIZE)
      self.GetDlcGenerator().VerifyImageSize()

  def testGetOptimalImageBlockSize(self):
    """Test that GetOptimalImageBlockSize returns the valid block size."""
    dlc_generator = self.GetDlcGenerator()
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(0), 0)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(1), 1)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(_BLOCK_SIZE), 1)
    self.assertEqual(dlc_generator.GetOptimalImageBlockSize(_BLOCK_SIZE + 1), 2)


class FinalizeDlcsTest(cros_test_lib.MockTempDirTestCase):
  """Tests functions that generate the final DLC images."""

  def setUp(self):
    """Setup FinalizeDlcsTest."""
    self.ExpectRootOwnedFiles()

  def testInstallDlcImages(self):
    """Tests InstallDlcImages to make sure all DLCs are copied correctly"""
    sysroot = os.path.join(self.tempdir, 'sysroot')
    osutils.WriteFile(
        os.path.join(sysroot, _IMAGE_DIR, _ID, 'pkg', build_dlc.DLC_IMAGE),
        'content',
        makedirs=True)
    osutils.SafeMakedirs(os.path.join(sysroot, _IMAGE_DIR, _ID, 'pkg'))
    output = os.path.join(self.tempdir, 'output')
    build_dlc.InstallDlcImages(board=_BOARD, sysroot=sysroot,
                               install_root_dir=output)
    self.assertExists(os.path.join(output, _ID, 'pkg', build_dlc.DLC_IMAGE))

  def testInstallDlcImagesNoDlc(self):
    copy_contents_mock = self.PatchObject(osutils, 'CopyDirContents')
    sysroot = os.path.join(self.tempdir, 'sysroot')
    output = os.path.join(self.tempdir, 'output')
    build_dlc.InstallDlcImages(board=_BOARD, sysroot=sysroot,
                               install_root_dir=output)
    copy_contents_mock.assert_not_called()

  def testInstallDlcImagesWithPreloadAllowed(self):
    package_nums = 2
    preload_allowed_json = '{"preload-allowed": true}'
    sysroot = os.path.join(self.tempdir, 'sysroot')
    for package_num in range(package_nums):
      osutils.WriteFile(
          os.path.join(sysroot, _IMAGE_DIR, _ID, _PACKAGE + str(package_num),
                       build_dlc.DLC_IMAGE),
          'image content',
          makedirs=True)
      osutils.WriteFile(
          os.path.join(sysroot, build_dlc.DLC_BUILD_DIR, _ID,
                       _PACKAGE + str(package_num), build_dlc.DLC_TMP_META_DIR,
                       build_dlc.IMAGELOADER_JSON),
          preload_allowed_json,
          makedirs=True)
    output = os.path.join(self.tempdir, 'output')
    build_dlc.InstallDlcImages(board=_BOARD, sysroot=sysroot,
                               install_root_dir=output, preload=True)
    for package_num in range(package_nums):
      self.assertExists(
          os.path.join(output, _ID, _PACKAGE + str(package_num),
                       build_dlc.DLC_IMAGE))

  def testInstallDlcImagesWithPreloadNotAllowed(self):
    package_nums = 2
    preload_not_allowed_json = '{"preload-allowed": false}'
    sysroot = os.path.join(self.tempdir, 'sysroot')
    for package_num in range(package_nums):
      osutils.WriteFile(
          os.path.join(sysroot, _IMAGE_DIR, _ID, _PACKAGE + str(package_num),
                       build_dlc.DLC_IMAGE),
          'image content',
          makedirs=True)
      osutils.WriteFile(
          os.path.join(sysroot, build_dlc.DLC_BUILD_DIR, _ID,
                       _PACKAGE + str(package_num), build_dlc.DLC_TMP_META_DIR,
                       build_dlc.IMAGELOADER_JSON),
          preload_not_allowed_json,
          makedirs=True)
    output = os.path.join(self.tempdir, 'output')
    build_dlc.InstallDlcImages(board=_BOARD, sysroot=sysroot,
                               install_root_dir=output, preload=True)
    for package_num in range(package_nums):
      self.assertNotExists(
          os.path.join(output, _ID, _PACKAGE + str(package_num),
                       build_dlc.DLC_IMAGE))

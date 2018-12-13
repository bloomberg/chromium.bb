# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS imagefile signing unittests"""

from __future__ import print_function

import os

import mock

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import image_lib_unittest
from chromite.lib import osutils
from chromite.signing.lib import firmware
from chromite.signing.lib import keys
from chromite.signing.image_signing import imagefile


# pylint: disable=protected-access

DEFAULT_VB_PATH = os.path.join(
    constants.SOURCE_ROOT, 'src/platform/vboot_reference/scripts/image_signing')

class TestPathForVbootSigningScripts(cros_test_lib.MockTestCase):
  """Tests for _PathForVbootSigningScripts"""

  def testDefault(self):
    """Test default value for path works."""
    path = imagefile._PathForVbootSigningScripts()
    self.assertEqual(DEFAULT_VB_PATH, path['PATH'].split(':')[0])

  def testPathPassed(self):
    """Test that passed path is used."""
    path = imagefile._PathForVbootSigningScripts(path='F/G')
    self.assertEqual('F/G', path['PATH'].split(':')[0])

  def testDefaultPathAlreadyPresent(self):
    """Test no change when path is already present."""
    os.environ['PATH'] += ':' + DEFAULT_VB_PATH
    path = imagefile._PathForVbootSigningScripts()
    self.assertEqual(os.environ['PATH'], path['PATH'])

  def testPathAlreadyPresent(self):
    """Test no change when path is already present."""
    value = os.environ['PATH'].split(':')
    path = imagefile._PathForVbootSigningScripts(path=value[1])
    self.assertEqual(os.environ['PATH'], path['PATH'])


class TestSignImage(cros_test_lib.RunCommandTempDirTestCase):
  """Test SignImage function."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)
    self.PatchObject(image_lib, 'LoopbackPartitions', return_value=self.image)
    self.fw_mock = self.PatchObject(firmware, 'ResignImageFirmware')
    self.android_mock = self.PatchObject(imagefile, 'SignAndroidImage')
    self.uefi_mock = self.PatchObject(imagefile, 'SignUefiBinaries')
    self.PatchObject(imagefile, '_PathForVbootSigningScripts',
                     return_value={'PATH': 'path'})

  def testSimple(self):
    """Test that USB case works, and strips boot."""
    imagefile.SignImage('USB', 'infile', 'outfile', 2, '/keydir')
    self.android_mock.assert_called_once()
    rootfs_dir, keyset = self.android_mock.call_args[0]
    self.fw_mock.assert_called_once_with('outfile', keyset)
    expected_rc = [
        mock.call(['cp', '--sparse=always', 'infile', 'outfile']),
        mock.call(['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
                  capture_output=True, print_cmd=False),
        mock.call(['strip_boot_from_image.sh', '--image', '/dev/loop9999p3'],
                  extra_env={'PATH': 'path'}),
    ]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.assertTrue(rootfs_dir.startswith(self.tempdir + '/'))
    self.assertTrue(rootfs_dir.endswith('/dir-3'))
    self.assertEqual('/keydir', keyset.key_dir)
    self.uefi_mock.assert_called_once_with(
        self.image, rootfs_dir, keyset, vboot_path=None)

  def testNoStripOnNonFactrory(self):
    """Verify that strip is not called on factory installs."""
    imagefile.SignImage('factory_install', 'infile', 'outfile', 2, '/keydir')
    self.assertNotIn(
        mock.call(['strip_boot_from_image.sh', '--image', '/dev/loop9999p3'],
                  extra_env={'PATH': 'path'}),
        self.rc.call_args_list)

  def testNoStripOnLegacy(self):
    """Verify that strip is not called for legacy."""
    self.rc.AddCmdResult(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        output=' cros_legacy ')
    imagefile.SignImage('USB', 'infile', 'outfile', 2, '/keydir')
    self.assertNotIn(
        mock.call(['strip_boot_from_image.sh', '--image', '/dev/loop9999p3'],
                  extra_env={'PATH': 'path'}),
        self.rc.call_args_list)

  def testNoStripOnEFI(self):
    """Verify that strip is not called for EFI."""
    self.rc.AddCmdResult(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        output=' cros_efi ')
    imagefile.SignImage('USB', 'infile', 'outfile', 2, '/keydir')
    self.assertNotIn(
        mock.call(['strip_boot_from_image.sh', '--image', '/dev/loop9999p3'],
                  extra_env={'PATH': 'path'}),
        self.rc.call_args_list)


class TestSignAndroidImage(cros_test_lib.RunCommandTempDirTestCase):
  """Test SignAndroidImage function."""

  def setUp(self):
    self.keytempdir = osutils.TempDir()
    self.keyset = keys.Keyset(self.keytempdir.tempdir)
    self.info_mock = self.PatchObject(logging, 'info')
    self.warn_mock = self.PatchObject(logging, 'warning')
    self.rc.SetDefaultCmdResult()
    self.PatchObject(imagefile, '_PathForVbootSigningScripts',
                     return_value={'PATH': 'path'})

  def testNoImage(self):
    """Test with no Android image."""
    exists_mock = self.PatchObject(os.path, 'exists', return_value=False)
    imagefile.SignAndroidImage(self.tempdir, self.keyset)
    exists_mock.assert_called_once_with(
        os.path.join(self.tempdir,
                     'opt/google/containers/android/system.raw.img'))
    self.info_mock.assert_called_once_with(
        'ARC image not found.  Not signing Android APKs.')
    self.assertEqual(0, self.rc.call_count)

  def testNoVersion(self):
    """Test: have Android image, but no ARC_VERSION info."""
    exists_mock = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(cros_build_lib, 'LoadKeyValueFile', return_value={})
    imagefile.SignAndroidImage(self.tempdir, self.keyset)
    exists_mock.assert_called_once_with(
        os.path.join(self.tempdir,
                     'opt/google/containers/android/system.raw.img'))
    self.warn_mock.assert_called_once_with(
        'CHROMEOS_ARC_VERSION not found in lsb-release. '
        'Not signing Android APKs.')
    self.assertEqual(0, self.info_mock.call_count)
    self.assertEqual(0, self.rc.call_count)

  def testTriesToSign(self):
    """Test: have Android image, and Android version."""
    exists_mock = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(cros_build_lib, 'LoadKeyValueFile',
                     return_value={'CHROMEOS_ARC_VERSION': '9'})
    imagefile.SignAndroidImage(self.tempdir, self.keyset)
    exists_mock.assert_called_once_with(
        os.path.join(self.tempdir,
                     'opt/google/containers/android/system.raw.img'))
    self.assertEqual(0, self.warn_mock.call_count)
    android_keydir = '%s/android' % self.keyset.key_dir
    expected_info = [
        mock.call('Found ARC image version %s, resigning APKs', '9'),
        mock.call('Using %s', android_keydir),
    ]
    self.assertEqual(expected_info, self.info_mock.call_args_list)
    self.assertEqual(
        self.rc.call_args_list,
        [mock.call(['sign_android_image.sh', self.tempdir, android_keydir],
                   extra_env={'PATH': 'path'})])


class TestSignUefiBinaries(cros_test_lib.RunCommandTempDirTestCase):
  """Test SignUefiBinaries function."""

  def setUp(self):
    self.keytempdir = osutils.TempDir()
    self.keyset = keys.Keyset(self.keytempdir.tempdir)
    self.info_mock = self.PatchObject(logging, 'info')
    self.warn_mock = self.PatchObject(logging, 'warning')
    self.rc.SetDefaultCmdResult()
    self.image = image_lib_unittest.LoopbackPartitionsMock('meh', self.tempdir)
    self.PatchObject(imagefile, '_PathForVbootSigningScripts',
                     return_value={'PATH': 'path'})

  def testUefiKeydir(self):
    """Test with no uefi keys."""
    isdir_mock = self.PatchObject(os.path, 'isdir', return_value=False)
    imagefile.SignUefiBinaries(self.image, self.tempdir, self.keyset)
    uefi_keydir = os.path.join(self.keyset.key_dir, 'uefi')
    isdir_mock.assert_called_once_with(uefi_keydir)
    self.info_mock.assert_called_once_with('No UEFI keys in keyset. Skipping.')
    self.assertEqual(0, self.rc.call_count)

  def testNoEfiPartition(self):
    """Test: have uefi keys, but no EFI-SYSTEM partition."""
    isdir_mock = self.PatchObject(os.path, 'isdir', return_value=True)
    self.PatchObject(image_lib_unittest.LoopbackPartitionsMock, '_Mount',
                     side_effect=KeyError(repr('EFI_SYSTEM')))
    imagefile.SignUefiBinaries(self.image, self.tempdir, self.keyset)
    uefi_keydir = os.path.join(self.keyset.key_dir, 'uefi')
    isdir_mock.assert_called_once_with(uefi_keydir)
    self.info_mock.assert_called_once_with('No EFI-SYSTEM partition found.')
    self.assertEqual(0, self.rc.call_count)

  def testSigns(self):
    """Test with uefi keys and EFI-SYSTEM partition."""
    isdir_mock = self.PatchObject(os.path, 'isdir', return_value=True)
    imagefile.SignUefiBinaries(self.image, self.tempdir, self.keyset)
    uefi_keydir = os.path.join(self.keyset.key_dir, 'uefi')
    isdir_mock.assert_called_once_with(uefi_keydir)
    uefi_fsdir = os.path.join(self.tempdir, 'dir-12')
    expected_rc = [
        mock.call(['install_gsetup_certs.sh', uefi_fsdir, uefi_keydir],
                  extra_env={'PATH': 'path'}),
        mock.call(['sign_uefi.sh', uefi_fsdir, uefi_keydir],
                  extra_env={'PATH': 'path'}),
        mock.call([
            'sign_uefi.sh', os.path.join(self.tempdir, 'boot'), uefi_keydir],
                  extra_env={'PATH': 'path'}),
    ]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.info_mock.assert_called_once_with('Signed UEFI binaries.')

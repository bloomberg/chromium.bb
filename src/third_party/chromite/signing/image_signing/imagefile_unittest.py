# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS imagefile signing unittests"""

from __future__ import print_function

import os
import re
import tempfile

import mock
import six

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import image_lib_unittest
from chromite.lib import kernel_cmdline
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.signing.lib import firmware
from chromite.signing.lib import keys
from chromite.signing.image_signing import imagefile
from chromite.utils import key_value_store


# pylint: disable=protected-access


class CalculateRootfsHashMock(imagefile.CalculateRootfsHash):
  """Mock for CalculateRootfsHash, to make testing easier."""
  # pylint: disable=super-init-not-called
  def __init__(self, image, kern_cmdline, calc_dm_args=None, calc_conf=None):
    self.image = image
    self.kern_cmdline = kern_cmdline
    self._file = tempfile.NamedTemporaryFile(delete=False)
    if not calc_dm_args:
      calc_dm_args = '1 vroot none ro 1,0 800 verity alg=sha1'
    self.calculated_dm_config = kernel_cmdline.DmConfig(calc_dm_args)
    if calc_conf:
      self.calculated_kernel_cmdline = calc_conf
    else:
      self.calculated_kernel_cmdline = 'CALCULATED KERNEL CONFIG'
    self.hashtree_filename = self._file.name

  def __del__(self):
    osutils.SafeUnlink(self._file.name)


DEFAULT_VB_PATH = os.path.join(
    constants.SOURCE_ROOT, 'src/platform/vboot_reference/scripts/image_signing')

# Sample output from dump_kernel_config.
SAMPLE_KERNEL_CONFIG = (
    'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
    'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
    'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="1 vroot none ro 1,0 '
    '3891200 verity payload=PARTUUID=%U/PARTNROFF=1 '
    'hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=3891200 alg=sha1 '
    'root_hexdigest=9999999999999999999999999999999999999999 '
    'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" '
    'noinitrd cros_debug vt.global_cursor_default=0 kern_guid=%U '
    'add_efi_memmap boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 '
    'tpm_tis.interrupts=0 nmi_watchdog=panic,lapic disablevmx=off ')
SAMPLE_VERITY_OUTPUT = (
    '0 3891200 verity payload=ROOT_DEV hashtree=HASH_DEV hashstart=3891200 '
    'alg=sha1 root_hexdigest=9999999999999999999999999999999999999999 '
    'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb ')


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


class TestGetKernelConfig(cros_test_lib.RunCommandTestCase):
  """Tests for GetKernelConfig."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.rc.AddCmdResult(partial_mock.In('/dev/loop9999p3'), returncode=1)
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p4']),
        output=SAMPLE_KERNEL_CONFIG)

  def testCallsDumpKernelConfig(self):
    """Verify that it calls dump_kernel_config correctly."""
    ret = imagefile.GetKernelConfig('/dev/loop9999p4')
    expected_rc = [
        mock.call(['sudo', '--', 'dump_kernel_config', '/dev/loop9999p4'],
                  capture_output=True, print_cmd=False, check=True,
                  encoding='utf-8')]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.assertTrue(isinstance(ret, six.string_types))
    self.assertEqual(SAMPLE_KERNEL_CONFIG.strip(), ret)

  def testCallsPassesErrorCodeOk(self):
    """Verify that it passes error_code_ok."""
    ret = imagefile.GetKernelConfig('/dev/loop9999p4', check=555)
    expected_rc = [
        mock.call(['sudo', '--', 'dump_kernel_config', '/dev/loop9999p4'],
                  capture_output=True, print_cmd=False, check=555,
                  encoding='utf-8')]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.assertTrue(isinstance(ret, six.string_types))
    self.assertEqual(SAMPLE_KERNEL_CONFIG.strip(), ret)

  def testCallsHandlesErrorCode(self):
    """Verify that it handles errors."""
    with self.assertRaises(cros_build_lib.RunCommandError):
      imagefile.GetKernelConfig('/dev/loop9999p3')
    ret = imagefile.GetKernelConfig('/dev/loop9999p3', check=False)
    self.assertEqual(None, ret)


class TestGetKernelCmdLine(cros_test_lib.MockTestCase):
  """Tests for _GetKernelCmdLine."""

  def testCallsGetKernelConfig(self):
    """Verify that it calls GetKernelConfig correctly."""
    gkc = self.PatchObject(imagefile, 'GetKernelConfig',
                           return_value=SAMPLE_KERNEL_CONFIG.strip())
    ret = imagefile._GetKernelCmdLine('/dev/loop9999p4')
    gkc.assert_called_once_with('/dev/loop9999p4', True)
    self.assertTrue(isinstance(ret, kernel_cmdline.CommandLine))
    self.assertEqual(SAMPLE_KERNEL_CONFIG.strip(), ret.Format())

  def testCallsPassesErrorCodeOk(self):
    """Verify that it passes error_code_ok."""
    gkc = self.PatchObject(imagefile, 'GetKernelConfig',
                           return_value=SAMPLE_KERNEL_CONFIG.strip())
    ret = imagefile._GetKernelCmdLine('/dev/loop9999p4', check=555)
    gkc.assert_called_once_with('/dev/loop9999p4', 555)
    self.assertTrue(isinstance(ret, kernel_cmdline.CommandLine))
    self.assertEqual(SAMPLE_KERNEL_CONFIG.strip(), ret.Format())

  def testCallsHandlesNone(self):
    """Verify that it handles errors."""
    self.PatchObject(imagefile, 'GetKernelConfig', return_value=None)
    ret = imagefile._GetKernelCmdLine('/dev/loop9999p3', check=False)
    self.assertEqual(None, ret)


class TestSignImage(cros_test_lib.RunCommandTempDirTestCase):
  """Test SignImage()."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)
    self.PatchObject(image_lib, 'LoopbackPartitions', return_value=self.image)
    self.fw_mock = self.PatchObject(firmware, 'ResignImageFirmware')
    self.android_mock = self.PatchObject(imagefile, 'SignAndroidImage')
    self.uefi_mock = self.PatchObject(imagefile, 'SignUefiBinaries')
    self.PatchObject(imagefile, 'UpdateRootfsHash', return_value=True)
    self.PatchObject(imagefile, 'UpdateStatefulPartitionVblock')
    self.PatchObject(imagefile, 'UpdateRecoveryKernelHash', return_value=True)
    self.PatchObject(imagefile, 'UpdateLegacyBootloader', return_value=True)
    self.PatchObject(imagefile, '_PathForVbootSigningScripts',
                     return_value={'PATH': 'path'})

  def testSimple(self):
    """Test that USB case works, and strips boot."""
    imagefile.SignImage('USB', 'infile', 'outfile', 2, '/keydir')
    self.android_mock.assert_called_once()
    rootfs_dir, keyset = self.android_mock.call_args[0]
    self.fw_mock.assert_called_once_with('outfile', keyset)
    expected_rc = [
        mock.call(['cp', '--sparse=always', 'infile', 'outfile'],),
        mock.call(['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
                  capture_output=True, print_cmd=False, check=True,
                  encoding='utf-8'),
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
    self.rc.AddCmdResult(partial_mock.In('sign_android_image.sh'))
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
    self.PatchObject(key_value_store, 'LoadFile', return_value={})
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
    self.PatchObject(key_value_store, 'LoadFile',
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


class TestCalculateRootfsHash(cros_test_lib.RunCommandTempDirTestCase):
  """Test CalculateRootfsHash function and its supporting functions."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)
    self.PatchObject(image_lib, 'LoopbackPartitions', return_value=self.image)

  def testSimple(self):
    """Test the simple case for CalculateRootfsHash."""
    self.rc.AddCmdResult(
        partial_mock.In('verity'), output=SAMPLE_VERITY_OUTPUT)
    rootfs_hash = imagefile.CalculateRootfsHash(
        self.image, kernel_cmdline.CommandLine(SAMPLE_KERNEL_CONFIG))
    expected_rc = [
        mock.call(['sudo', '--', 'verity', 'mode=create', 'alg=sha1',
                   'payload=/dev/loop9999p3', 'payload_blocks=486400',
                   'hashtree=%s' % rootfs_hash.hashtree_filename,
                   'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
                   'bbbbbbbbbbbbbbb'], capture_output=True, print_cmd=False,
                  encoding='utf-8')]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.assertEqual(kernel_cmdline.DmConfig(
        '1 vroot none ro 1,0 3891200 verity '
        'payload=PARTUUID=%U/PARTNROFF=1 hashtree=PARTUUID=%U/PARTNROFF=1 '
        'hashstart=3891200 alg=sha1 '
        'root_hexdigest=9999999999999999999999999999999999999999 salt=bbbbb'
        'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'),
                     rootfs_hash.calculated_dm_config)
    self.assertEqual(
        'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
        'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
        'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="1 vroot none ro '
        '1,0 3891200 verity payload=PARTUUID=%U/PARTNROFF=1 '
        'hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=3891200 alg=sha1 '
        'root_hexdigest=9999999999999999999999999999999999999999 '
        'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
        'bbbb" noinitrd cros_debug vt.global_cursor_default=0 kern_guid=%U '
        'add_efi_memmap boot=local noresume noswap i915.modeset=1 '
        'tpm_tis.force=1 tpm_tis.interrupts=0 nmi_watchdog=panic,lapic '
        'disablevmx=off', rootfs_hash.calculated_kernel_cmdline.Format())

  def testTempfileDeletedOnDelete(self):
    """Test that the tempfile is deleted only when the object is deleted."""
    self.rc.AddCmdResult(
        partial_mock.In('verity'), output=SAMPLE_VERITY_OUTPUT)
    rootfs_hash = imagefile.CalculateRootfsHash(
        self.image, kernel_cmdline.CommandLine(SAMPLE_KERNEL_CONFIG))
    # We don't actually care about the return, it's checked elsewhere.
    file_name = rootfs_hash.hashtree_filename
    self.assertExists(file_name)
    del rootfs_hash
    self.assertNotExists(file_name)

  def testSaltOptional(self):
    """Test that salt= is properly optional."""
    self.rc.AddCmdResult(
        partial_mock.In('verity'),
        output=re.sub(' salt=b*', '', SAMPLE_VERITY_OUTPUT))
    kern_cmdline = re.sub(' salt=b*', '', SAMPLE_KERNEL_CONFIG)
    rootfs_hash = imagefile.CalculateRootfsHash(
        self.image, kernel_cmdline.CommandLine(kern_cmdline))
    expected_rc = [
        mock.call(['sudo', '--', 'verity', 'mode=create', 'alg=sha1',
                   'payload=/dev/loop9999p3', 'payload_blocks=486400',
                   'hashtree=%s' % rootfs_hash.hashtree_filename],
                  capture_output=True, print_cmd=False, encoding='utf-8')]
    self.assertEqual(expected_rc, self.rc.call_args_list)
    self.assertEqual(
        '1 vroot none ro 1,0 3891200 verity '
        'payload=PARTUUID=%U/PARTNROFF=1 hashtree=PARTUUID=%U/PARTNROFF=1 '
        'hashstart=3891200 alg=sha1 '
        'root_hexdigest=9999999999999999999999999999999999999999',
        rootfs_hash.calculated_dm_config.Format())
    self.assertEqual(
        'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
        'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
        'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="1 vroot none ro '
        '1,0 3891200 verity payload=PARTUUID=%U/PARTNROFF=1 '
        'hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=3891200 alg=sha1 '
        'root_hexdigest=9999999999999999999999999999999999999999" noinitrd '
        'cros_debug vt.global_cursor_default=0 kern_guid=%U add_efi_memmap '
        'boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 '
        'tpm_tis.interrupts=0 nmi_watchdog=panic,lapic disablevmx=off',
        rootfs_hash.calculated_kernel_cmdline.Format())


class TestClearResignFlag(cros_test_lib.RunCommandTempDirTestCase):
  """Test ClearResignFlag function and its supporting functions."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)

  def testUnlinksFile(self):
    self.PatchObject(os.path, 'exists', return_value=True)
    imagefile.ClearResignFlag(self.image)
    self.assertEqual(1, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'rm', '--',
         '%s/dir-3/root/.need_to_be_signed' % self.image.destination],
        redirect_stderr=True, print_cmd=False)


class TestUpdateRootfsHash(cros_test_lib.RunCommandTempDirTestCase):
  """Test UpdateRootfsHash function and its supporting functions."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG)
    self.keytempdir = osutils.TempDir()
    self.keyset = keys.Keyset(self.keytempdir.tempdir)
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)
    self.root_hash = CalculateRootfsHashMock('meh', SAMPLE_KERNEL_CONFIG)
    self.PatchObject(imagefile, 'CalculateRootfsHash',
                     return_value=self.root_hash)
    self.rc.AddCmdResult(
        partial_mock.In('tune2fs'), output='Block count: 4480\n')
    self.ukc = self.PatchObject(imagefile, '_UpdateKernelConfig')

  def testSimple(self):
    """Test the normal path"""
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p4']),
        output=SAMPLE_KERNEL_CONFIG)
    self.keyset.keys['keyA_kernel_data_key'] = keys.KeyPair(
        'keyA_kernel_data_key', self.keytempdir.tempdir)
    self.keyset.keys['kernel_data_key'] = keys.KeyPair(
        'kernel_data_key', self.keytempdir.tempdir)
    imagefile.UpdateRootfsHash(
        self.image, self.image.GetPartitionDevName('KERN-A'), self.keyset,
        'keyA_')
    self.assertEqual(5, self.rc.call_count)
    expected_kernel_cmdline = kernel_cmdline.CommandLine(
        'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
        'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
        'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="1 vroot none ro 1,0 '
        '800 verity alg=sha1" noinitrd cros_debug vt.global_cursor_default=0 '
        'kern_guid=%U add_efi_memmap boot=local noresume noswap '
        'i915.modeset=1 tpm_tis.force=1 tpm_tis.interrupts=0 '
        'nmi_watchdog=panic,lapic disablevmx=off')
    expected_calls = [
        mock.call('/dev/loop9999p2', expected_kernel_cmdline,
                  self.keyset.keys['keyA_kernel_data_key']),
        mock.call('/dev/loop9999p4', expected_kernel_cmdline,
                  self.keyset.keys['kernel_data_key']),
    ]
    self.assertEqual(expected_calls, self.ukc.call_args_list)

  def testMissingKernB(self):
    """Test the path where KERN-B fails to dump config"""
    self.keyset.keys['keyA_kernel_data_key'] = keys.KeyPair(
        'keyA_kernel_data_key', self.keytempdir.tempdir)
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG)
    self.rc.AddCmdResult(partial_mock.In('/dev/loop9999p4'), returncode=1)
    imagefile.UpdateRootfsHash(
        self.image, self.image.GetPartitionDevName('KERN-A'), self.keyset,
        'keyA_')
    self.assertEqual(5, self.rc.call_count)
    expected_kernel_cmdline = kernel_cmdline.CommandLine(
        'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
        'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
        'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="1 vroot none ro 1,0 '
        '800 verity alg=sha1" noinitrd cros_debug vt.global_cursor_default=0 '
        'kern_guid=%U add_efi_memmap boot=local noresume noswap '
        'i915.modeset=1 tpm_tis.force=1 tpm_tis.interrupts=0 '
        'nmi_watchdog=panic,lapic disablevmx=off')
    expected_calls = [
        mock.call('/dev/loop9999p2', expected_kernel_cmdline,
                  self.keyset.keys['keyA_kernel_data_key']),
    ]
    self.assertEqual(expected_calls, self.ukc.call_args_list)


class TestUpdateKernelConfig(cros_test_lib.RunCommandTestCase):
  """Test _UpdateKernelConfig."""

  def testSimple(self):
    self.rc.SetDefaultCmdResult()
    loop_kern = '/dev/loop9999p2'
    cmd_line = kernel_cmdline.CommandLine(SAMPLE_KERNEL_CONFIG)
    key = keys.KeyPair('key', '/keydir')
    imagefile._UpdateKernelConfig(loop_kern, cmd_line, key)
    self.assertEqual(1, self.rc.call_count)
    expected_cmdA = [
        'sudo', '--', 'vbutil_kernel', '--repack', loop_kern,
        '--keyblock', '/keydir/key.keyblock',
        '--signprivate', '/keydir/key.vbprivk',
        '--version', '1',
        '--oldblob', '/dev/loop9999p2',
        '--config', mock.ANY]
    self.rc.assertCommandCalled(expected_cmdA)


class TestUpdateStatefulVblock(cros_test_lib.RunCommandTempDirTestCase):
  """Test UpdateStatefulPartitionVblock function."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG)
    self.keytempdir = osutils.TempDir()
    self.keyset = keys.Keyset(self.keytempdir.tempdir)
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)

  def testSimple(self):
    """Test the normal path"""
    kernel_key = keys.KeyPair('kernel_data_key', self.keytempdir.tempdir)
    self.keyset.keys['kernel_data_key'] = kernel_key
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p4']),
        output=SAMPLE_KERNEL_CONFIG)
    imagefile.UpdateStatefulPartitionVblock(self.image, self.keyset)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p4'],
        print_cmd=False, capture_output=True, check=False, encoding='utf-8')
    self.rc.assertCommandCalled([
        'sudo', '--', 'vbutil_kernel', '--repack', mock.ANY,
        '--keyblock', kernel_key.keyblock,
        '--signprivate', kernel_key.private,
        '--oldblob', '/dev/loop9999p4', '--vblockonly'])
    self.rc.assertCommandCalled([
        'sudo', '--', 'cp', mock.ANY,
        '%s/dir-1/vmlinuz_hd.vblock' % self.tempdir])

  def testNoKernB(self):
    """Test the normal path"""
    kernel_key = keys.KeyPair('kernel_data_key', self.keytempdir.tempdir)
    self.keyset.keys['kernel_data_key'] = kernel_key
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p4']),
        returncode=1)
    imagefile.UpdateStatefulPartitionVblock(self.image, self.keyset)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p4'],
        print_cmd=False, capture_output=True, check=False, encoding='utf-8')
    self.rc.assertCommandCalled([
        'sudo', '--', 'vbutil_kernel', '--repack', mock.ANY,
        '--keyblock', kernel_key.keyblock,
        '--signprivate', kernel_key.private,
        '--oldblob', '/dev/loop9999p2', '--vblockonly'])
    self.rc.assertCommandCalled([
        'sudo', '--', 'cp', mock.ANY,
        '%s/dir-1/vmlinuz_hd.vblock' % self.tempdir])


class TestUpdateRecoveryKernelHash(cros_test_lib.RunCommandTempDirTestCase):
  """Test UpdateRecoveryKernelHash function."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.expected_sha1sum = '5' * 40
    self.rc.AddCmdResult(
        partial_mock.In('sha1sum'), output=self.expected_sha1sum + '  meh')
    self.loginfo = self.PatchObject(logging, 'info')
    self.keytempdir = osutils.TempDir()
    self.keyset = keys.Keyset(self.keytempdir.tempdir)
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)
    self.ukc = self.PatchObject(imagefile, '_UpdateKernelConfig')

  def testSimple(self):
    """Test the normal path"""
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG + 'kern_b_hash=888888888 ')
    recovery = keys.KeyPair('recovery', self.keytempdir.tempdir, version=3)
    self.keyset.keys['recovery'] = recovery
    imagefile.UpdateRecoveryKernelHash(self.image, self.keyset)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    new_cmdline = (
        SAMPLE_KERNEL_CONFIG +
        'kern_b_hash=5555555555555555555555555555555555555555')
    self.loginfo.assert_called_once_with(
        'New cmdline for kernel A is %s', new_cmdline)
    self.ukc.assert_called_once_with(
        '/dev/loop9999p2', kernel_cmdline.CommandLine(new_cmdline), recovery)

  def testNoKernBHash(self):
    """Test no KERN-B hash case."""
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG)
    recovery = keys.KeyPair('recovery', self.keytempdir.tempdir, version=3)
    self.keyset.keys['recovery'] = recovery
    imagefile.UpdateRecoveryKernelHash(self.image, self.keyset)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    new_cmdline = SAMPLE_KERNEL_CONFIG.strip()
    self.loginfo.assert_called_once_with(
        'New cmdline for kernel A is %s', new_cmdline)
    self.ukc.assert_called_once_with(
        '/dev/loop9999p2', kernel_cmdline.CommandLine(new_cmdline), recovery)


class TestUpdateLegacyBootloader(cros_test_lib.RunCommandTempDirTestCase):
  """Test UpdateLegacyBootloader function."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.rc.AddCmdResult(
        partial_mock.InOrder(['dump_kernel_config', '/dev/loop9999p2']),
        output=SAMPLE_KERNEL_CONFIG)
    self.image = image_lib_unittest.LoopbackPartitionsMock(
        'outfile', self.tempdir)

  def _CreateCfgFiles(self, syslinux, efiboot):
    """Create the directory structure for testing UpdateLegacyBootLoader."""
    uefi_dir = os.path.join(self.tempdir, 'dir-12')
    if syslinux:
      sys_cfgs = [os.path.join(uefi_dir, 'syslinux', cfg) for cfg in (
          'bif.cfg', 'foo.cfg')]
      sys_other = [os.path.join(uefi_dir, 'syslinux', cfg) for cfg in (
          'bar.baz', 'notcfg')]
    else:
      sys_cfgs = []
      sys_other = []
    if efiboot:
      path = os.path.join(uefi_dir, 'efi/boot/grub.cfg')
      sys_cfgs.append(path)
    for cfg in sys_cfgs + sys_other:
      osutils.Touch(cfg, makedirs=True)
    return {
        'uefi_dir': uefi_dir,
        'sys_cfgs': sys_cfgs,
        'sys_other': sys_other}

  def testSimple(self):
    """Test the normal path"""
    uefi = self._CreateCfgFiles(True, True)
    imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p2')
    self.assertEqual(2, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    sed_command = self.rc.call_args_list[1]
    sed_args = (sed_command[0][0][:5] + sorted(sed_command[0][0][5:]),)
    sed_args += sed_command[0][1:]
    self.assertEqual(
        (['sudo', '--', 'sed', '-iE',
          's/\\broot_hexdigest=[a-z0-9]+/root_hexdigest=9999999999999999999'
          '999999999999999999999/g'] + sorted(uefi['sys_cfgs']), ), sed_args)
    self.assertEqual({'error_code_ok': True}, sed_command[1])

  def testNoSyslinux(self):
    """Test with no syslinux/."""
    uefi = self._CreateCfgFiles(False, True)
    imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p2')
    self.assertEqual(2, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    sed_command = self.rc.call_args_list[1]
    sed_args = (sed_command[0][0][:5] + sorted(sed_command[0][0][5:]),)
    sed_args += sed_command[0][1:]
    self.assertEqual(
        (['sudo', '--', 'sed', '-iE', 's/\\broot_hexdigest=[a-z0-9]+/root_h'
          'exdigest=9999999999999999999999999999999999999999/g'] +
         sorted(uefi['sys_cfgs']), ), sed_args)
    self.assertEqual({'error_code_ok': True}, sed_command[1])

  def testNoGrubCfg(self):
    """Test with no efi/boot/grub.cfg."""
    uefi = self._CreateCfgFiles(True, False)
    imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p2')
    self.assertEqual(2, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    sed_command = self.rc.call_args_list[1]
    sed_args = (sed_command[0][0][:5] + sorted(sed_command[0][0][5:]),)
    sed_args += sed_command[0][1:]
    self.assertEqual(
        (['sudo', '--', 'sed', '-iE', 's/\\broot_hexdigest=[a-z0-9]+/root_h'
          'exdigest=9999999999999999999999999999999999999999/g'] +
         sorted(uefi['sys_cfgs']), ), sed_args)
    self.assertEqual({'error_code_ok': True}, sed_command[1])

  def testNoSyslinuxSedFails(self):
    """Test no syslinux/"""
    uefi = self._CreateCfgFiles(False, True)
    self.rc.AddCmdResult(partial_mock.In('sed'), returncode=1)
    with self.assertRaises(imagefile.SignImageError):
      imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p2')
    self.assertEqual(2, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    sed_command = self.rc.call_args_list[1]
    sed_args = (sed_command[0][0][:5] + sorted(sed_command[0][0][5:]),)
    sed_args += sed_command[0][1:]
    self.assertEqual(
        (['sudo', '--', 'sed', '-iE', 's/\\broot_hexdigest=[a-z0-9]+/root_h'
          'exdigest=9999999999999999999999999999999999999999/g'] +
         sorted(uefi['sys_cfgs']), ), sed_args)
    self.assertEqual({'error_code_ok': True}, sed_command[1])

  def testNoGrubCfgSedFails(self):
    """Test the normal path"""
    uefi = self._CreateCfgFiles(True, False)
    self.rc.AddCmdResult(partial_mock.In('sed'), returncode=1)
    with self.assertRaises(imagefile.SignImageError):
      imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p2')
    self.assertEqual(2, self.rc.call_count)
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p2'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')
    sed_command = self.rc.call_args_list[1]
    sed_args = (sed_command[0][0][:5] + sorted(sed_command[0][0][5:]),)
    sed_args += sed_command[0][1:]
    self.assertEqual(
        (['sudo', '--', 'sed', '-iE', 's/\\broot_hexdigest=[a-z0-9]+/root_h'
          'exdigest=9999999999999999999999999999999999999999/g'] +
         sorted(uefi['sys_cfgs']), ), sed_args)
    self.assertEqual({'error_code_ok': True}, sed_command[1])

  def testNoKernelConfig(self):
    """Test the normal path"""
    with self.assertRaises(imagefile.SignImageError) as e:
      imagefile.UpdateLegacyBootloader(self.image, '/dev/loop9999p4')
    # Empty kernel cmdline raises this error.
    self.assertEqual('Could not find root digest', str(e.exception))
    self.rc.assertCommandCalled(
        ['sudo', '--', 'dump_kernel_config', '/dev/loop9999p4'],
        capture_output=True, print_cmd=False, check=True, encoding='utf-8')


class TestDumpConfig(cros_test_lib.MockTestCase):
  """Test DumpConfig() function."""

  def testSimple(self):
    """Test the normal case."""
    image = image_lib_unittest.LoopbackPartitionsMock('outfile')
    self.PatchObject(image_lib, 'LoopbackPartitions', return_value=image)
    gkc = self.PatchObject(imagefile, 'GetKernelConfig', return_value='Config')
    imagefile.DumpConfig('image.bin')
    expected = [
        mock.call('/dev/loop9999p2', check=False),
        mock.call('/dev/loop9999p4', check=False),
    ]
    self.assertEqual(expected, gkc.call_args_list)

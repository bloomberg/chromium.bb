# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the image_lib module."""

from __future__ import print_function

import collections
import gc
import glob
import os
import stat

import mock

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import retry_util
from chromite.lib import partial_mock

# pylint: disable=protected-access

class FakeException(Exception):
  """Fake exception used for testing exception handling."""


FAKE_PATH = '/imaginary/file'
LOOP_DEV = '/dev/loop9999'
LOOP_PART_COUNT = 12
LOOP_PARTITION_INFO = [
    image_lib.PartitionInfo(
        1, 2928640, 2957311, 28672, 14680064, 'STATE', ''),
    image_lib.PartitionInfo(
        2, 20480, 53247, 32768, 16777216, 'KERN-A', ''),
    image_lib.PartitionInfo(
        3, 286720, 2928639, 2641920, 1352663040, 'ROOT-A', ''),
    image_lib.PartitionInfo(
        4, 53248, 86015, 32768, 16777216, 'KERN-B', ''),
    image_lib.PartitionInfo(
        5, 282624, 286719, 4096, 2097152, 'ROOT-B', ''),
    image_lib.PartitionInfo(
        6, 16448, 16448, 1, 512, 'KERN-C', ''),
    image_lib.PartitionInfo(
        7, 16449, 16449, 1, 512, 'ROOT-C', ''),
    image_lib.PartitionInfo(
        8, 86016, 118783, 32768, 16777216, 'OEM', ''),
    image_lib.PartitionInfo(
        9, 16450, 16450, 1, 512, 'reserved', ''),
    image_lib.PartitionInfo(
        10, 16451, 16451, 1, 512, 'reserved', ''),
    image_lib.PartitionInfo(
        11, 64, 16447, 16384, 8388608, 'RWFW', ''),
    image_lib.PartitionInfo(
        12, 249856, 282623, 32768, 16777216, 'EFI-SYSTEM', ''),
]
LOOP_PARTS_DICT = {
    p.number: '%sp%d' % (LOOP_DEV, p.number) for p in LOOP_PARTITION_INFO}
LOOP_PARTS_LIST = LOOP_PARTS_DICT.values()

class LoopbackPartitionsMock(image_lib.LoopbackPartitions):
  """Mocked loopback partition class to use in unit tests."""
  # pylint: disable=super-init-not-called
  def __init__(self, path, destination=None, part_ids=None, mount_opts=None,
               dev=LOOP_DEV, part_count=0):
    """Initialize.

    Args:
      (shared with LoopbackPartitions)
      path: Path to the image file.
      destination: destination directory.
      part_ids: Mount these partitions at context manager entry.
      mount_opts: Use these mount_opts for mounting |part_ids|.
      (unique to LoopbackPartitionsMock)
      dev: Path for the base loopback device.
      part_count: How many partition device files to make up.  Default: normal
          partition table.
    """
    self.path = path
    self.dev = dev
    self.part_ids = part_ids
    self.mount_opts = mount_opts
    if destination:
      self.destination = destination
    else:
      self.destination = osutils.TempDir()
    if part_count:
      self._gpt_table = [
          image_lib.PartitionInfo(num, 0, 0, 0, '', 'my-%d' % num, '')
          for num in range(1, part_count + 1)]
    else:
      self._gpt_table = LOOP_PARTITION_INFO
    self.parts = {p.number: '%sp%s' % (dev, p.number)
                  for p in self._gpt_table}
    self.enable_rw_called = set()
    self.disable_rw_called = set()
  # pylint: enable=super-init-not-called

  def EnableRwMount(self, part_id, offset=0):
    """Stub out enable rw mount."""
    self.enable_rw_called.add((part_id, offset))

  def DisableRwMount(self, part_id, offset=0):
    """Stub out disable rw mount."""
    self.disable_rw_called.add((part_id, offset))

  def _Mount(self, part, mount_opts):
    """Stub out mount operations."""
    dest_number, _ = self._GetMountPointAndSymlink(part)
    # Don't actually even try to mount it, let alone mark it mounted.
    return dest_number

  def _Unmount(self, part):
    """Stub out unmount operations."""

  def close(self):
    pass


class LoopbackPartitionsTest(cros_test_lib.MockTempDirTestCase):
  """Test the loopback partitions class"""

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.StartPatcher(self.rc_mock)
    self.rc_mock.SetDefaultCmdResult()
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)

    self.PatchObject(image_lib, 'GetImageDiskPartitionInfo',
                     return_value=LOOP_PARTITION_INFO)
    self.PatchObject(glob, 'glob', return_value=LOOP_PARTS_LIST)
    self.mount_mock = self.PatchObject(osutils, 'MountDir')
    self.umount_mock = self.PatchObject(osutils, 'UmountDir')
    self.retry_mock = self.PatchObject(retry_util, 'RetryException')
    def fake_which(val, *_arg, **_kwargs):
      return val
    self.PatchObject(osutils, 'Which', side_effect=fake_which)

  def testContextManager(self):
    """Test using the loopback class as a context manager."""
    with image_lib.LoopbackPartitions(FAKE_PATH) as lb:
      self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
      self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
      self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
      self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                         expected=False)
      self.assertEqual(lb.parts, LOOP_PARTS_DICT)
      self.assertEqual(lb._gpt_table, LOOP_PARTITION_INFO)
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def testContextManagerWithMounts(self):
    """Test using the loopback class as a context manager with mounts."""
    syml = self.PatchObject(osutils, 'SafeSymlink')
    part_ids = (1, 'ROOT-A')
    with image_lib.LoopbackPartitions(
        FAKE_PATH, part_ids=part_ids, mount_opts=('ro',)) as lb:
      expected_mounts = set()
      expected_calls = []
      for part_id in part_ids:
        for part in LOOP_PARTITION_INFO:
          if part.name == part_id or part.number == part_id:
            expected_mounts.add(part)
            expected_calls.append(
                mock.call('dir-%d' % part.number, os.path.join(
                    lb.destination, 'dir-%s' % part.name)))
            break
      self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
      self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
      self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
      self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                         expected=False)
      self.assertEqual(lb.parts, LOOP_PARTS_DICT)
      self.assertEqual(lb._gpt_table, LOOP_PARTITION_INFO)
      self.assertEqual(expected_calls, syml.call_args_list)
      self.assertEqual(expected_mounts, lb._mounted)
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])


  def testManual(self):
    """Test using the loopback class closed manually."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEqual(lb.parts, LOOP_PARTS_DICT)
    self.assertEqual(lb._gpt_table, LOOP_PARTITION_INFO)
    lb.close()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def gcFunc(self):
    """This function isolates a local variable so it'll be garbage collected."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEqual(lb.parts, LOOP_PARTS_DICT)
    self.assertEqual(lb._gpt_table, LOOP_PARTITION_INFO)

  def testGarbageCollected(self):
    """Test using the loopback class closed by garbage collection."""
    self.gcFunc()
    # Force garbage collection in case python didn't already clean up the
    # loopback object.
    gc.collect()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def testMountUnmount(self):
    """Test Mount() and Unmount() entry points."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH, destination=self.tempdir)
    # Mount four partitions.
    lb.Mount((1, 3, 'ROOT-B', 'ROOT-C'))
    for p in (1, 3, 5, 7):
      self.mount_mock.assert_any_call(
          '%sp%d' % (LOOP_DEV, p), '%s/dir-%d' % (self.tempdir, p),
          makedirs=True, skip_mtab=False, sudo=True, mount_opts=('ro',))
      linkname = '%s/dir-%s' % (self.tempdir, LOOP_PARTITION_INFO[p - 1].name)
      self.assertTrue(stat.S_ISLNK(os.lstat(linkname).st_mode))
    self.assertEqual(4, self.mount_mock.call_count)
    self.umount_mock.assert_not_called()

    # Unmount half of them, confirm that they were unmounted.
    lb.Unmount((1, 'ROOT-B'))
    for p in (1, 5):
      self.umount_mock.assert_any_call('%s/dir-%d' % (self.tempdir, p),
                                       cleanup=False)
    self.assertEqual(2, self.umount_mock.call_count)
    self.umount_mock.reset_mock()

    # Close the object, so that we unmount the other half of them.
    lb.close()
    for p in (3, 7):
      self.umount_mock.assert_any_call('%s/dir-%d' % (self.tempdir, p),
                                       cleanup=False)
    self.assertEqual(2, self.umount_mock.call_count)

    # Verify that the directories were cleaned up.
    for p in (1, 3):
      self.retry_mock.assert_any_call(
          cros_build_lib.RunCommandError, 60, osutils.RmDir,
          '%s/dir-%d' % (self.tempdir, p), sudo=True, sleep=1)

  def testMountingMountedPartReturnsName(self):
    """Test that Mount returns the directory name even when already mounted."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH, destination=self.tempdir)
    dirname = '%s/dir-%d' % (self.tempdir, lb._gpt_table[0].number)
    # First make sure we get the directory name when we actually mount.
    self.assertEqual(dirname, lb._Mount(lb._gpt_table[0], ('ro',)))
    # Then make sure we get it when we call it again.
    self.assertEqual(dirname, lb._Mount(lb._gpt_table[0], ('ro',)))

  def testRemountCallsMount(self):
    """Test that Mount returns the directory name even when already mounted."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH, destination=self.tempdir)
    devname = '%sp%d' % (LOOP_DEV, lb._gpt_table[0].number)
    dirname = '%s/dir-%d' % (self.tempdir, lb._gpt_table[0].number)
    # First make sure we get the directory name when we actually mount.
    self.assertEqual(dirname, lb._Mount(lb._gpt_table[0], ('ro',)))
    self.mount_mock.assert_called_once_with(
        devname, dirname,
        makedirs=True, skip_mtab=False, sudo=True, mount_opts=('ro',))
    # Then make sure we get it when we call it again.
    self.assertEqual(dirname, lb._Mount(lb._gpt_table[0], ('remount', 'rw')))
    self.assertEqual(
        mock.call(devname, dirname, makedirs=True, skip_mtab=False,
                  sudo=True, mount_opts=('remount', 'rw')),
        self.mount_mock.call_args)

  def testGetPartitionDevName(self):
    """Test GetPartitionDevName()."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    for part in LOOP_PARTITION_INFO:
      self.assertEqual('%sp%d' % (LOOP_DEV, part.number),
                       lb.GetPartitionDevName(part.number))
      if part.name != 'reserved':
        self.assertEqual('%sp%d' % (LOOP_DEV, part.number),
                         lb.GetPartitionDevName(part.name))

  def test_GetMountPointAndSymlink(self):
    """Test _GetMountPointAndSymlink()."""
    lb = image_lib.LoopbackPartitions(FAKE_PATH, destination=self.tempdir)
    for part in LOOP_PARTITION_INFO:
      expected = [os.path.join(lb.destination, 'dir-%s' % n)
                  for n in (part.number, part.name)]
      self.assertEqual(expected, list(lb._GetMountPointAndSymlink(part)))

  def testIsExt2OnVarious(self):
    """Test _IsExt2 works with the various partition types."""
    FS_PARTITIONS = (1, 3, 8)
    # STATE, ROOT-A, and OEM generally have ext2 filesystems.
    for x in FS_PARTITIONS:
      self.rc_mock.AddCmdResult(
          partial_mock.In('if=%sp%d' % (LOOP_DEV, x)),
          output=b'\x53\xef')
    # Throw errors on all of the partitions that are < 1000 bytes.
    for part in LOOP_PARTITION_INFO:
      if part.size < 1000:
        self.rc_mock.AddCmdResult(
            partial_mock.In('if=%sp%d' % (LOOP_DEV, part.number)),
            returncode=1, error='Seek failed\n')
    lb = image_lib.LoopbackPartitions(FAKE_PATH, destination=self.tempdir)
    # We expect that only the partitions in FS_PARTITIONS are ext2.
    self.assertEqual(
        [part.number in FS_PARTITIONS for part in LOOP_PARTITION_INFO],
        [lb._IsExt2(part.name) for part in LOOP_PARTITION_INFO])

class LsbUtilsTest(cros_test_lib.MockTempDirTestCase):
  """Tests the various LSB utilities."""

  def setUp(self):
    # Patch os.getuid(..) to pretend running as root, so reading/writing the
    # lsb-release file doesn't require escalated privileges and the test can
    # clean itself up correctly.
    self.PatchObject(os, 'getuid', return_value=0)

  def testWriteLsbRelease(self):
    """Tests writing out the lsb_release file using WriteLsbRelease(..)."""
    rc_mock = self.PatchObject(cros_build_lib, 'sudo_run')
    fields = collections.OrderedDict((
        ('x', '1'), ('y', '2'), ('foo', 'bar'),
    ))
    image_lib.WriteLsbRelease(self.tempdir, fields)
    lsb_release_file = os.path.join(self.tempdir, 'etc', 'lsb-release')
    expected_content = 'x=1\ny=2\nfoo=bar\n'
    self.assertFileContents(lsb_release_file, expected_content)
    rc_mock.assert_called_once_with([
        'setfattr', '-n', 'security.selinux', '-v',
        'u:object_r:cros_conf_file:s0',
        os.path.join(self.tempdir, 'etc/lsb-release')])

    # Test that WriteLsbRelease(..) correctly handles an existing file.
    rc_mock = self.PatchObject(cros_build_lib, 'sudo_run')
    fields = collections.OrderedDict((
        ('newkey1', 'value1'), ('newkey2', 'value2'), ('a', '3'), ('b', '4'),
    ))
    image_lib.WriteLsbRelease(self.tempdir, fields)
    expected_content = ('x=1\ny=2\nfoo=bar\nnewkey1=value1\nnewkey2=value2\n'
                        'a=3\nb=4\n')
    self.assertFileContents(lsb_release_file, expected_content)
    rc_mock.assert_called_once_with([
        'setfattr', '-n', 'security.selinux', '-v',
        'u:object_r:cros_conf_file:s0',
        os.path.join(self.tempdir, 'etc/lsb-release')])


class BuildImagePathTest(cros_test_lib.MockTempDirTestCase):
  """BuildImagePath tests."""

  def setUp(self):
    self.board = 'board'
    self.board_dir = os.path.join(self.tempdir, self.board)

    D = cros_test_lib.Directory
    filesystem = (
        D(self.board, ('recovery_image.bin', 'other_image.bin')),
        'full_path_image.bin',
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    self.full_path = os.path.join(self.tempdir, 'full_path_image.bin')

  def testBuildImagePath(self):
    """BuildImagePath tests."""
    self.PatchObject(image_lib, 'GetLatestImageLink',
                     return_value=os.path.join(self.tempdir, self.board))

    # Board and full image path provided.
    result = image_lib.BuildImagePath(self.board, self.full_path)
    self.assertEqual(self.full_path, result)

    # Only full image path provided.
    result = image_lib.BuildImagePath(None, self.full_path)
    self.assertEqual(self.full_path, result)

    # Full image path provided that does not exist.
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(self.board, '/does/not/exist')
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(None, '/does/not/exist')

    # Default image is used.
    result = image_lib.BuildImagePath(self.board, None)
    self.assertEqual(os.path.join(self.board_dir, 'recovery_image.bin'), result)

    # Image basename provided.
    result = image_lib.BuildImagePath(self.board, 'other_image.bin')
    self.assertEqual(os.path.join(self.board_dir, 'other_image.bin'), result)

    # Image basename provided that does not exist.
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(self.board, 'does_not_exist.bin')

    default_mock = self.PatchObject(cros_build_lib, 'GetDefaultBoard')

    # Nothing provided, and no default.
    default_mock.return_value = None
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(None, None)

    # Nothing provided, with default.
    default_mock.return_value = 'board'
    result = image_lib.BuildImagePath(None, None)
    self.assertEqual(os.path.join(self.board_dir, 'recovery_image.bin'), result)


class SecurityTestConfigTest(cros_test_lib.RunCommandTempDirTestCase):
  """SecurityTestConfig class tests."""

  # pylint: disable=protected-access

  def setUp(self):
    self.image = '/path/to/image.bin'
    self.baselines = '/path/to/baselines'
    self.vboot_hash = 'abc123'
    self.config = image_lib.SecurityTestConfig(self.image, self.baselines,
                                               self.vboot_hash, self.tempdir)

  def testVbootCheckout(self):
    """Test normal flow - clone and checkout."""
    clone_patch = self.PatchObject(git, 'Clone')
    self.config._VbootCheckout()
    clone_patch.assert_called_once()
    self.assertCommandContains(['git', 'checkout', self.vboot_hash])

    # Make sure it doesn't try to clone & checkout again after already having
    # done so successfully.
    clone_patch = self.PatchObject(git, 'Clone')
    self.config._VbootCheckout()
    clone_patch.assert_not_called()

  def testVbootCheckoutError(self):
    """Test exceptions in a git command."""
    rce = cros_build_lib.RunCommandError('error')
    self.PatchObject(git, 'Clone', side_effect=rce)
    with self.assertRaises(image_lib.VbootCheckoutError):
      self.config._VbootCheckout()

  def testVbootCheckoutNoDirectory(self):
    """Test the error handling when the directory does not exist."""
    # Test directory that does not exist.
    self.config.directory = '/DOES/NOT/EXIST'
    with self.assertRaises(image_lib.SecurityConfigDirectoryError):
      self.config._VbootCheckout()

  def testRunCheck(self):
    """RunCheck tests."""
    # No config argument when running check.
    self.config.RunCheck('check1', False)
    check1 = os.path.join(self.config._checks_dir, 'ensure_check1.sh')
    config1 = os.path.join(self.baselines, 'ensure_check1.config')
    self.assertCommandContains([check1, self.image])
    self.assertCommandContains([config1], expected=False)

    # Include config argument when running check.
    self.config.RunCheck('check2', True)
    check2 = os.path.join(self.config._checks_dir, 'ensure_check2.sh')
    config2 = os.path.join(self.baselines, 'ensure_check2.config')
    self.assertCommandContains([check2, self.image, config2])


class GetImageDiskPartitionInfoTests(cros_test_lib.RunCommandTestCase):
  """Tests the GetImageDiskPartitionInfo function."""

  SAMPLE_PARTED = """/foo/chromiumos_qemu_image.bin:\
2271240192B:file:512:512:gpt::;
11:32768B:8421375B:8388608B::RWFW:;
6:8421376B:8421887B:512B::KERN-C:;
7:8421888B:8422399B:512B::ROOT-C:;
9:8422400B:8422911B:512B::reserved:;
10:8422912B:8423423B:512B::reserved:;
2:10485760B:27262975B:16777216B::KERN-A:;
4:27262976B:44040191B:16777216B::KERN-B:;
8:44040192B:60817407B:16777216B:ext4:OEM:msftdata;
12:127926272B:161480703B:33554432B:fat16:EFI-SYSTEM:boot, esp;
5:161480704B:163577855B:2097152B::ROOT-B:;
3:163577856B:2260729855B:2097152000B:ext2:ROOT-A:;
1:2260729856B:2271215615B:10485760B:ext2:STATE:msftdata;
"""

  SAMPLE_CGPT = """
       start        size    part  contents
           0           1          PMBR (Boot GUID: 88FB7EB8-2B3F-B943-B933-\
EEC571FFB6E1)
           1           1          Pri GPT header
           2          32          Pri GPT table
     1921024     2097152       1  Label: "STATE"
                                  Type: Linux data
                                  UUID: EEBD83BE-397E-BD44-878B-0DDDD5A5C510
       20480       32768       2  Label: "KERN-A"
                                  Type: ChromeOS kernel
                                  UUID: 7007C2F3-08E5-AB40-A4BC-FF5B01F5460D
                                  Attr: priority=15 tries=15 successful=1
     1101824      819200       3  Label: "ROOT-A"
                                  Type: ChromeOS rootfs
                                  UUID: F4C5C3AD-027F-894B-80CD-3DEC57932948
       53248       32768       4  Label: "KERN-B"
                                  Type: ChromeOS kernel
                                  UUID: C85FB478-404C-8741-ADB8-11312A35880D
                                  Attr: priority=0 tries=0 successful=0
      282624      819200       5  Label: "ROOT-B"
                                  Type: ChromeOS rootfs
                                  UUID: A99F4231-1EC3-C542-AC0C-DF3729F5DB07
       16448           1       6  Label: "KERN-C"
                                  Type: ChromeOS kernel
                                  UUID: 81F0E336-FAC9-174D-A08C-864FE627B637
                                  Attr: priority=0 tries=0 successful=0
       16449           1       7  Label: "ROOT-C"
                                  Type: ChromeOS rootfs
                                  UUID: 9E127FCA-30C1-044E-A5F2-DF74E6932692
       86016       32768       8  Label: "OEM"
                                  Type: Linux data
                                  UUID: 72986347-A37C-684F-9A19-4DBAF41C55A9
       16450           1       9  Label: "reserved"
                                  Type: ChromeOS reserved
                                  UUID: BA85A0A7-1850-964D-8EF8-6707AC106C3A
       16451           1      10  Label: "reserved"
                                  Type: ChromeOS reserved
                                  UUID: 16C9EC9B-50FA-DD46-98DC-F781360817B4
          64       16384      11  Label: "RWFW"
                                  Type: ChromeOS firmware
                                  UUID: BE8AECB9-4F78-7C44-8F23-5A9273B7EC8F
      249856       32768      12  Label: "EFI-SYSTEM"
                                  Type: EFI System Partition
                                  UUID: 88FB7EB8-2B3F-B943-B933-EEC571FFB6E1
     4050847          32          Sec GPT table
     4050879           1          Sec GPT header
"""

  def testCgpt(self):
    """Tests that we can list all partitions with `cgpt` correctly."""
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.SAMPLE_CGPT)
    partitions = image_lib.GetImageDiskPartitionInfo('...')
    part_dict = {p.name: p for p in partitions}
    self.assertEqual(part_dict['STATE'].start, 983564288)
    self.assertEqual(part_dict['STATE'].size, 1073741824)
    self.assertEqual(part_dict['STATE'].number, 1)
    self.assertEqual(part_dict['STATE'].name, 'STATE')
    self.assertEqual(part_dict['EFI-SYSTEM'].start, 249856 * 512)
    self.assertEqual(part_dict['EFI-SYSTEM'].size, 32768 * 512)
    self.assertEqual(part_dict['EFI-SYSTEM'].number, 12)
    self.assertEqual(part_dict['EFI-SYSTEM'].name, 'EFI-SYSTEM')
    self.assertEqual(12, len(partitions))

  def testNormalPath(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.SAMPLE_PARTED)
    partitions = image_lib.GetImageDiskPartitionInfo('_ignored')
    part_dict = {p.name: p for p in partitions}
    self.assertEqual(12, len(partitions))
    self.assertEqual(1, part_dict['STATE'].number)
    self.assertEqual(2097152000, part_dict['ROOT-A'].size)

  def testKeyedByNumber(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.SAMPLE_PARTED)
    partitions = image_lib.GetImageDiskPartitionInfo(
        '_ignored'
    )
    part_dict = {p.number: p for p in partitions}
    self.assertEqual(12, len(part_dict))
    self.assertEqual('STATE', part_dict[1].name)
    self.assertEqual(2097152000, part_dict[3].size)
    self.assertEqual('reserved', part_dict[9].name)
    self.assertEqual('reserved', part_dict[10].name)

  def testChangeUnitInsideChroot(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.SAMPLE_CGPT)
    partitions = image_lib.GetImageDiskPartitionInfo('_ignored')
    part_dict = {p.name: p for p in partitions}
    self.assertEqual(part_dict['STATE'].start, 983564288)
    self.assertEqual(part_dict['STATE'].size, 1073741824)

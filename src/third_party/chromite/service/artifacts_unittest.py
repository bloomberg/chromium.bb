# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Artifacts service tests."""

from __future__ import print_function

import json
import mock
import os
import shutil

from chromite.lib import autotest_util
from chromite.lib import build_target_util
from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import sysroot_lib
from chromite.lib import toolchain_util
from chromite.lib.paygen import partition_lib
from chromite.lib.paygen import paygen_payload_lib
from chromite.lib.paygen import paygen_stateful_payload_lib
from chromite.service import artifacts


class BundleAutotestFilesTest(cros_test_lib.MockTempDirTestCase):
  """Test the Bundle Autotest Files function."""

  def setUp(self):
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    self.archive_dir = os.path.join(self.tempdir, 'archive_base_dir')

    sysroot_path = os.path.join(self.tempdir, 'sysroot')
    sysroot_dne = os.path.join(self.tempdir, 'sysroot_DNE')
    self.sysroot = sysroot_lib.Sysroot(sysroot_path)
    self.sysroot_dne = sysroot_lib.Sysroot(sysroot_dne)

    # Make sure we have the valid paths.
    osutils.SafeMakedirs(self.output_dir)
    osutils.SafeMakedirs(sysroot_path)

  def testInvalidOutputDirectory(self):
    """Test invalid output directory."""
    with self.assertRaises(AssertionError):
      artifacts.BundleAutotestFiles(self.sysroot, None)

  def testInvalidSysroot(self):
    """Test sysroot that does not exist."""
    with self.assertRaises(AssertionError):
      artifacts.BundleAutotestFiles(self.sysroot_dne, self.output_dir)

  def testArchiveDirectoryDoesNotExist(self):
    """Test archive directory that does not exist causes error."""
    with self.assertRaises(artifacts.ArchiveBaseDirNotFound):
      artifacts.BundleAutotestFiles(self.sysroot, self.output_dir)

  def testSuccess(self):
    """Test a successful call handling."""
    ab_path = os.path.join(self.sysroot.path, constants.AUTOTEST_BUILD_PATH)
    osutils.SafeMakedirs(ab_path)

    # Makes all of the individual calls to build out each of the tarballs work
    # nicely with a single patch.
    self.PatchObject(autotest_util.AutotestTarballBuilder, '_BuildTarball',
                     side_effect=lambda _, path, **kwargs: osutils.Touch(path))

    result = artifacts.BundleAutotestFiles(self.sysroot, self.output_dir)

    for archive in result.values():
      self.assertStartsWith(archive, self.output_dir)
      self.assertExists(archive)


class ArchiveChromeEbuildEnvTest(cros_test_lib.MockTempDirTestCase):
  """ArchiveChromeEbuildEnv tests."""

  def setUp(self):
    # Create the chroot and sysroot instances.
    self.chroot_path = os.path.join(self.tempdir, 'chroot_dir')
    self.chroot = chroot_lib.Chroot(path=self.chroot_path)
    self.sysroot_path = os.path.join(self.chroot_path, 'sysroot_dir')
    self.sysroot = sysroot_lib.Sysroot(self.sysroot_path)

    # Create the output directory.
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

    # The sysroot's /var/db/pkg prefix for the chrome package directories.
    var_db_pkg = os.path.join(self.sysroot_path, 'var', 'db', 'pkg')
    # Create the var/db/pkg dir so we have that much for no-chrome tests.
    osutils.SafeMakedirs(var_db_pkg)

    # Two versions of chrome to test the multiple version checks/handling.
    chrome_v1 = '%s-1.0.0-r1' % constants.CHROME_PN
    chrome_v2 = '%s-2.0.0-r1' % constants.CHROME_PN

    # Build the two chrome version paths.
    chrome_cat_dir = os.path.join(var_db_pkg, constants.CHROME_CN)
    self.chrome_v1_dir = os.path.join(chrome_cat_dir, chrome_v1)
    self.chrome_v2_dir = os.path.join(chrome_cat_dir, chrome_v2)

    # Directory tuple for verifying the result archive contents.
    self.expected_archive_contents = cros_test_lib.Directory('./',
                                                             'environment')

    # Create a environment.bz2 file to put into folders.
    env_file = os.path.join(self.tempdir, 'environment')
    osutils.Touch(env_file)
    cros_build_lib.RunCommand(['bzip2', env_file])
    self.env_bz2 = '%s.bz2' % env_file

  def _CreateChromeDir(self, path, populate=True):
    """Setup a chrome package directory.

    Args:
      path (str): The full chrome package path.
      populate (bool): Whether to include the environment bz2.
    """
    osutils.SafeMakedirs(path)
    if populate:
      shutil.copy(self.env_bz2, path)

  def testSingleChromeVersion(self):
    """Test a successful single-version run."""
    self._CreateChromeDir(self.chrome_v1_dir)

    created = artifacts.ArchiveChromeEbuildEnv(self.sysroot, self.output_dir)

    self.assertStartsWith(created, self.output_dir)
    cros_test_lib.VerifyTarball(created, self.expected_archive_contents)

  def testMultipleChromeVersions(self):
    """Test a successful multiple version run."""
    # Create both directories, but don't populate the v1 dir so it'll hit an
    # error if the wrong one is used.
    self._CreateChromeDir(self.chrome_v1_dir, populate=False)
    self._CreateChromeDir(self.chrome_v2_dir)

    created = artifacts.ArchiveChromeEbuildEnv(self.sysroot, self.output_dir)

    self.assertStartsWith(created, self.output_dir)
    cros_test_lib.VerifyTarball(created, self.expected_archive_contents)

  def testNoChrome(self):
    """Test no version of chrome present."""
    with self.assertRaises(artifacts.NoFilesError):
      artifacts.ArchiveChromeEbuildEnv(self.sysroot, self.output_dir)


class CreateChromeRootTest(cros_test_lib.RunCommandTempDirTestCase):
  """CreateChromeRoot tests."""

  def setUp(self):
    # Create the build target.
    self.build_target = build_target_util.BuildTarget('board')

    # Create the chroot.
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    self.chroot_tmp = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(self.chroot_tmp)
    self.chroot = chroot_lib.Chroot(path=self.chroot_dir)

    # Create the output directory.
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

  def testRunCommandError(self):
    """Test handling when the run command call is not successful."""
    result = cros_build_lib.CommandResult()
    self.rc.SetDefaultCmdResult(
        side_effect=cros_build_lib.RunCommandError('Error', result))

    with self.assertRaises(artifacts.CrosGenerateSysrootError):
      artifacts.CreateChromeRoot(self.chroot, self.build_target,
                                 self.output_dir)

  def testSuccess(self):
    """Test success case."""
    # Separate tempdir for the method itself.
    call_tempdir = os.path.join(self.chroot_tmp, 'cgs_call_tempdir')
    osutils.SafeMakedirs(call_tempdir)
    self.PatchObject(osutils.TempDir, '__enter__', return_value=call_tempdir)

    # Set up files in the tempdir since the command isn't being called to
    # generate anything for it to handle.
    files = ['file1', 'file2', 'file3']
    expected_files = [os.path.join(self.output_dir, f) for f in files]
    for f in files:
      osutils.Touch(os.path.join(call_tempdir, f))

    created = artifacts.CreateChromeRoot(self.chroot, self.build_target,
                                         self.output_dir)

    # Just test the command itself and the parameter-based args.
    self.assertCommandContains(['cros_generate_sysroot',
                                '--board', self.build_target.name])
    # Make sure we
    self.assertItemsEqual(expected_files, created)
    for f in created:
      self.assertExists(f)


class BuildFirmwareArchiveTest(cros_test_lib.TempDirTestCase):
  """BuildFirmwareArchive tests."""

  def testBuildFirmwareArchive(self):
    """Verifies that firmware archiver includes proper files"""
    # Assorted set of file names, some of which are supposed to be included in
    # the archive.
    fw_files = (
        'dts/emeraldlake2.dts',
        'image-link.rw.bin',
        'nv_image-link.bin',
        'pci8086,0166.rom',
        'seabios.cbfs',
        'u-boot.elf',
        'u-boot_netboot.bin',
        'updater-link.rw.sh',
        'x86-memtest',
    )

    # Files which should be included in the archive.
    fw_archived_files = fw_files + ('dts/',)
    board = 'link'
    # fw_test_root = os.path.join(self.tempdir, os.path.basename(__file__))
    fw_test_root = self.tempdir
    fw_files_root = os.path.join(fw_test_root,
                                 'chroot/build/%s/firmware' % board)
    # Generate a representative set of files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(fw_files_root, fw_files)

    # Create the chroot and sysroot instances.
    chroot_path = os.path.join(self.tempdir, 'chroot')
    chroot = chroot_lib.Chroot(path=chroot_path)
    sysroot = sysroot_lib.Sysroot('/build/link')

    # Create an archive from the simulated firmware directory
    tarball = os.path.join(
        fw_test_root,
        artifacts.BuildFirmwareArchive(chroot, sysroot, fw_test_root))

    # Verify the tarball contents.
    cros_test_lib.VerifyTarball(tarball, fw_archived_files)


class BundleOrderfileGenerationArtifactsTest(cros_test_lib.MockTempDirTestCase):
  """BundleOrderfileGenerationArtifacts tests."""

  def setUp(self):
    # Create the build target.
    self.build_target = build_target_util.BuildTarget('board')

    # Create the chroot.
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    self.chroot_tmp = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(self.chroot_tmp)
    self.chroot = chroot_lib.Chroot(path=self.chroot_dir)

    # Create the output directory.
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

    # Create a dummy Chrome version
    self.chrome_version = '%s-1.0.0-r1' % constants.CHROME_PN

  def testSuccess(self):
    """Test success case."""
    # Separate tempdir for the method itself.
    call_tempdir = os.path.join(self.chroot_tmp, 'orderfile_call_tempdir')
    osutils.SafeMakedirs(call_tempdir)
    self.PatchObject(osutils.TempDir, '__enter__', return_value=call_tempdir)

    # Set up files in the tempdir since the command isn't being called to
    # generate anything for it to handle.
    files = [self.chrome_version+'.orderfile.tar.xz',
             self.chrome_version+'.nm.tar.xz']
    expected_files = [os.path.join(self.output_dir, f) for f in files]

    for f in files:
      osutils.Touch(os.path.join(call_tempdir, f))

    mock_generate = self.PatchObject(
        toolchain_util, 'GenerateChromeOrderfile',
        autospec=True)

    created = artifacts.BundleOrderfileGenerationArtifacts(
        self.chroot, self.build_target, self.chrome_version, self.output_dir)

    # Test the class is called with right arguments
    mock_generate.assert_called_with(
        self.build_target.name,
        call_tempdir,
        self.chrome_version,
        self.chroot.path,
        self.chroot.GetEnterArgs()
    )

    # Make sure we get all the expected files
    self.assertItemsEqual(expected_files, created)
    for f in created:
      self.assertExists(f)


class FetchPinnedGuestImagesTest(cros_test_lib.TempDirTestCase):
  """FetchPinnedGuestImages tests."""

  def setUp(self):
    self.chroot = chroot_lib.Chroot(self.tempdir)
    self.sysroot = sysroot_lib.Sysroot('/sysroot')
    sysroot_path = os.path.join(self.tempdir, 'sysroot')
    osutils.SafeMakedirs(sysroot_path)

    self.pin_dir = os.path.join(sysroot_path, constants.GUEST_IMAGES_PINS_PATH)
    osutils.SafeMakedirs(self.pin_dir)

  def testSuccess(self):
    """Tests that generating a guest images tarball."""
    for filename in ('file1', 'file2'):
      pin_file = os.path.join(self.pin_dir, filename + '.json')
      with open(pin_file, 'w') as f:
        pin = {
            'filename': filename + '.tar.gz',
            'gsuri': 'gs://%s' % filename,
        }
        json.dump(pin, f)

    expected = [
        artifacts.PinnedGuestImage(filename='file1.tar.gz', uri='gs://file1'),
        artifacts.PinnedGuestImage(filename='file2.tar.gz', uri='gs://file2'),
    ]

    pins = artifacts.FetchPinnedGuestImages(self.chroot, self.sysroot)
    self.assertItemsEqual(expected, pins)

  def testBadPin(self):
    """Tests that generating a guest images tarball with a bad pin file."""
    pin_file = os.path.join(self.pin_dir, 'file1.json')
    with open(pin_file, 'w') as f:
      pin = {
          'gsuri': 'gs://%s' % 'file1',
      }
      json.dump(pin, f)

    pins = artifacts.FetchPinnedGuestImages(self.chroot, self.sysroot)
    self.assertFalse(pins)

  def testNoPins(self):
    """Tests that generating a guest images tarball with no pins."""
    pins = artifacts.FetchPinnedGuestImages(self.chroot, self.sysroot)
    self.assertFalse(pins)


class GeneratePayloadsTest(cros_test_lib.MockTempDirTestCase):
  """Test cases for the payload generation functions."""

  def setUp(self):
    self.target_image = os.path.join(
        self.tempdir,
        'link/R37-5952.0.2014_06_12_2302-a1/chromiumos_test_image.bin')
    osutils.Touch(self.target_image, makedirs=True)

  def testGenerateFullTestPayloads(self):
    """Verifies correctly generating full payloads."""
    paygen_mock = self.PatchObject(paygen_payload_lib, 'GenerateUpdatePayload')
    artifacts.GenerateTestPayloads(self.target_image, self.tempdir, full=True)
    payload_path = os.path.join(
        self.tempdir,
        'chromeos_R37-5952.0.2014_06_12_2302-a1_link_full_dev.bin')
    paygen_mock.assert_call_once_with(self.target_image, payload_path)

  def testGenerateDeltaTestPayloads(self):
    """Verifies correctly generating delta payloads."""
    paygen_mock = self.PatchObject(paygen_payload_lib, 'GenerateUpdatePayload')
    artifacts.GenerateTestPayloads(self.target_image, self.tempdir, delta=True)
    payload_path = os.path.join(
        self.tempdir,
        'chromeos_R37-5952.0.2014_06_12_2302-a1_R37-'
        '5952.0.2014_06_12_2302-a1_link_delta_dev.bin')
    paygen_mock.assert_call_once_with(self.target_image, payload_path)

  def testGenerateStatefulTestPayloads(self):
    """Verifies correctly generating stateful payloads."""
    paygen_mock = self.PatchObject(paygen_stateful_payload_lib,
                                   'GenerateStatefulPayload')
    artifacts.GenerateTestPayloads(self.target_image, self.tempdir,
                                   stateful=True)
    paygen_mock.assert_call_once_with(self.target_image, self.tempdir)

  def testGenerateQuickProvisionPayloads(self):
    """Verifies correct files are created for quick_provision script."""
    extract_kernel_mock = self.PatchObject(partition_lib, 'ExtractKernel')
    extract_root_mock = self.PatchObject(partition_lib, 'ExtractRoot')
    compress_file_mock = self.PatchObject(cros_build_lib, 'CompressFile')

    artifacts.GenerateQuickProvisionPayloads(self.target_image, self.tempdir)

    extract_kernel_mock.assert_called_once_with(
        self.target_image, partial_mock.HasString('full_dev_part_KERN.bin'))
    extract_root_mock.assert_called_once_with(
        self.target_image, partial_mock.HasString('full_dev_part_ROOT.bin'),
        truncate=False)

    calls = [mock.call(partial_mock.HasString('full_dev_part_KERN.bin'),
                       partial_mock.HasString('full_dev_part_KERN.bin.gz')),
             mock.call(partial_mock.HasString('full_dev_part_ROOT.bin'),
                       partial_mock.HasString('full_dev_part_ROOT.bin.gz'))]
    compress_file_mock.assert_has_calls(calls)

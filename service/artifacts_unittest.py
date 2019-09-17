# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Artifacts service tests."""

from __future__ import print_function

import json
import os
import shutil

import mock

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
    self.chroot = chroot_lib.Chroot(self.tempdir)
    self.sysroot = sysroot_lib.Sysroot('sysroot')
    self.sysroot_dne = sysroot_lib.Sysroot('sysroot_DNE')

    # Make sure we have the valid paths.
    osutils.SafeMakedirs(self.output_dir)
    osutils.SafeMakedirs(sysroot_path)

  def testInvalidOutputDirectory(self):
    """Test invalid output directory."""
    with self.assertRaises(AssertionError):
      artifacts.BundleAutotestFiles(self.chroot, self.sysroot, None)

  def testInvalidSysroot(self):
    """Test sysroot that does not exist."""
    with self.assertRaises(AssertionError):
      artifacts.BundleAutotestFiles(self.chroot, self.sysroot_dne,
                                    self.output_dir)

  def testArchiveDirectoryDoesNotExist(self):
    """Test archive directory that does not exist causes error."""
    with self.assertRaises(artifacts.ArchiveBaseDirNotFound):
      artifacts.BundleAutotestFiles(self.chroot, self.sysroot, self.output_dir)

  def testSuccess(self):
    """Test a successful call handling."""
    ab_path = os.path.join(self.tempdir, self.sysroot.path,
                           constants.AUTOTEST_BUILD_PATH)
    osutils.SafeMakedirs(ab_path)

    # Makes all of the individual calls to build out each of the tarballs work
    # nicely with a single patch.
    self.PatchObject(autotest_util.AutotestTarballBuilder, '_BuildTarball',
                     side_effect=lambda _, path, **kwargs: osutils.Touch(path))

    result = artifacts.BundleAutotestFiles(self.chroot, self.sysroot,
                                           self.output_dir)

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


class ArchiveImagesTest(cros_test_lib.TempDirTestCase):
  """ArchiveImages tests."""

  def setUp(self):
    self.image_dir = os.path.join(self.tempdir, 'images')
    osutils.SafeMakedirs(self.image_dir)
    self.output_dir = os.path.join(self.tempdir, 'output')
    osutils.SafeMakedirs(self.output_dir)

    self.images = []
    for img in artifacts.IMAGE_TARS.keys():
      full_path = os.path.join(self.image_dir, img)
      self.images.append(full_path)
      osutils.Touch(full_path)

    osutils.Touch(os.path.join(self.image_dir, 'irrelevant_image.bin'))
    osutils.Touch(os.path.join(self.image_dir, 'foo.txt'))
    osutils.Touch(os.path.join(self.image_dir, 'bar'))

  def testNoImages(self):
    """Test an empty directory handling."""
    artifacts.ArchiveImages(self.tempdir, self.output_dir)
    self.assertFalse(os.listdir(self.output_dir))

  def testAllImages(self):
    """Test each image gets picked up."""
    created = artifacts.ArchiveImages(self.image_dir, self.output_dir)
    self.assertItemsEqual(artifacts.IMAGE_TARS.values(), created)


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


class BundleEBuildLogsTarballTest(cros_test_lib.TempDirTestCase):
  """BundleEBuildLogsTarball tests."""

  def testBundleEBuildLogsTarball(self):
    """Verifies that the correct EBuild tar files are bundled."""
    board = 'samus'
    # Create chroot object and sysroot object
    chroot_path = os.path.join(self.tempdir, 'chroot')
    chroot = chroot_lib.Chroot(path=chroot_path)
    sysroot_path = os.path.join('build', board)
    sysroot = sysroot_lib.Sysroot(sysroot_path)

    # Create parent dir for logs
    log_parent_dir = os.path.join(chroot.path, 'build')

    # Names of log files typically found in a build directory.
    log_files = (
        '',
        'x11-libs:libdrm-2.4.81-r24:20170816-175008.log',
        'x11-libs:libpciaccess-0.12.902-r2:20170816-174849.log',
        'x11-libs:libva-1.7.1-r2:20170816-175019.log',
        'x11-libs:libva-intel-driver-1.7.1-r4:20170816-175029.log',
        'x11-libs:libxkbcommon-0.4.3-r2:20170816-174908.log',
        'x11-libs:pango-1.32.5-r1:20170816-174954.log',
        'x11-libs:pixman-0.32.4:20170816-174832.log',
        'x11-misc:xkeyboard-config-2.15-r3:20170816-174908.log',
        'x11-proto:kbproto-1.0.5:20170816-174849.log',
        'x11-proto:xproto-7.0.31:20170816-174849.log',
    )
    tarred_files = [os.path.join('logs', x) for x in log_files]
    log_files_root = os.path.join(log_parent_dir,
                                  '%s/tmp/portage/logs' % board)
    # Generate a representative set of log files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(log_files_root, log_files)

    archive_dir = self.tempdir
    tarball = artifacts.BundleEBuildLogsTarball(chroot, sysroot, archive_dir)
    self.assertEqual('ebuild_logs.tar.xz', tarball)

    # Verify the tarball contents.
    tarball_fullpath = os.path.join(self.tempdir, tarball)
    cros_test_lib.VerifyTarball(tarball_fullpath, tarred_files)


class BundleChromeOSConfigTest(cros_test_lib.TempDirTestCase):
  """BundleChromeOSConfig tests."""

  def setUp(self):
    self.board = 'samus'

    # Create chroot object and sysroot object
    chroot_path = os.path.join(self.tempdir, 'chroot')
    self.chroot = chroot_lib.Chroot(path=chroot_path)
    sysroot_path = os.path.join('build', self.board)
    self.sysroot = sysroot_lib.Sysroot(sysroot_path)

    self.archive_dir = self.tempdir

  def testBundleChromeOSConfig(self):
    """Verifies that the correct ChromeOS config file is bundled."""
    # Create parent dir for ChromeOS Config output.
    config_parent_dir = os.path.join(self.chroot.path, 'build')

    # Names of ChromeOS Config files typically found in a build directory.
    config_files = ('config.json',
                    cros_test_lib.Directory('yaml', [
                        'config.c', 'config.yaml', 'ec_config.c', 'ec_config.h',
                        'model.yaml', 'private-model.yaml'
                    ]))
    config_files_root = os.path.join(
        config_parent_dir, '%s/usr/share/chromeos-config' % self.board)
    # Generate a representative set of config files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(config_files_root, config_files)

    # Write a payload to the config.yaml file.
    test_config_payload = {
        'chromeos': {
            'configs': [{
                'identity': {
                    'platform-name': 'Samus'
                }
            }]
        }
    }
    with open(os.path.join(config_files_root, 'yaml', 'config.yaml'), 'w') as f:
      json.dump(test_config_payload, f)

    config_filename = artifacts.BundleChromeOSConfig(self.chroot, self.sysroot,
                                                     self.archive_dir)
    self.assertEqual('config.yaml', config_filename)

    with open(os.path.join(self.archive_dir, config_filename), 'r') as f:
      self.assertEqual(test_config_payload, json.load(f))

  def testNoChromeOSConfigFound(self):
    """Verifies that None is returned when no ChromeOS config file is found."""
    self.assertIsNone(
        artifacts.BundleChromeOSConfig(self.chroot, self.sysroot,
                                       self.archive_dir))


class BundleVmFilesTest(cros_test_lib.TempDirTestCase):
  """BundleVmFiles tests."""

  def testBundleVmFiles(self):
    """Verifies that the correct files are bundled"""
    # Create the chroot instance.
    chroot_path = os.path.join(self.tempdir, 'chroot')
    chroot = chroot_lib.Chroot(path=chroot_path)

    # Create the test_results_dir
    test_results_dir = 'test/results'

    # Create a set of files where some should get bundled up as VM files.
    # Add a suffix (123) to one of the files matching the VM pattern prefix.
    vm_files = ('file1.txt',
                'file2.txt',
                'chromiumos_qemu_disk.bin' + '123',
                'chromiumos_qemu_mem.bin'
               )

    target_test_dir = os.path.join(chroot_path, test_results_dir)
    cros_test_lib.CreateOnDiskHierarchy(target_test_dir, vm_files)

    # Create the output directory.
    output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(output_dir)

    archives = artifacts.BundleVmFiles(
        chroot, test_results_dir, output_dir)
    expected_archive_files = [
        output_dir + '/chromiumos_qemu_disk.bin' + '123.tar',
        output_dir + '/chromiumos_qemu_mem.bin.tar']
    self.assertItemsEqual(archives, expected_archive_files)


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


class BundleAFDOGenerationArtifacts(cros_test_lib.MockTempDirTestCase):
  """BundleAFDOGenerationArtifacts tests."""

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

  def testRunSuccess(self):
    """Generic function for testing success cases for different types."""

    # Separate tempdir for the method itself.
    call_tempdir = os.path.join(self.chroot_tmp, 'call_tempdir')
    osutils.SafeMakedirs(call_tempdir)
    self.PatchObject(osutils.TempDir, '__enter__', return_value=call_tempdir)

    mock_orderfile_generate = self.PatchObject(
        toolchain_util, 'GenerateChromeOrderfile',
        autospec=True)

    mock_afdo_generate = self.PatchObject(
        toolchain_util, 'GenerateBenchmarkAFDOProfile',
        autospec=True)

    #Test both orderfile and AFDO.
    for is_orderfile in [False, True]:
      # Set up files in the tempdir since the command isn't being called to
      # generate anything for it to handle.
      files = ['artifact1', 'artifact2']
      expected_files = [os.path.join(self.output_dir, f) for f in files]
      for f in files:
        osutils.Touch(os.path.join(call_tempdir, f))

      created = artifacts.BundleAFDOGenerationArtifacts(
          is_orderfile, self.chroot, self.build_target, self.output_dir)

      # Test right class is called with right arguments
      if is_orderfile:
        mock_orderfile_generate.assert_called_once_with(
            board=self.build_target.name,
            output_dir=call_tempdir,
            chroot_path=self.chroot.path,
            chroot_args=self.chroot.GetEnterArgs()
        )
      else:
        mock_afdo_generate.assert_called_once_with(
            board=self.build_target.name,
            output_dir=call_tempdir,
            chroot_path=self.chroot.path,
            chroot_args=self.chroot.GetEnterArgs(),
        )

      # Make sure we get all the expected files
      self.assertItemsEqual(expected_files, created)
      for f in created:
        self.assertExists(f)
        os.remove(f)


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


class GenerateCpeExportTest(cros_test_lib.RunCommandTempDirTestCase):
  """GenerateCpeExport tests."""

  def setUp(self):
    self.sysroot = sysroot_lib.Sysroot('/build/board')
    self.chroot = chroot_lib.Chroot(self.tempdir)

    self.chroot_tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(self.chroot, 'tempdir', return_value=self.chroot_tempdir)

    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

    result_file = artifacts.CPE_RESULT_FILE_TEMPLATE % 'board'
    self.result_file = os.path.join(self.output_dir, result_file)

    warnings_file = artifacts.CPE_WARNINGS_FILE_TEMPLATE % 'board'
    self.warnings_file = os.path.join(self.output_dir, warnings_file)

  def testSuccess(self):
    """Test success handling."""
    # Set up warning output and the file the command would be making.
    report = 'Report.'
    warnings = 'Warnings.'
    self.rc.SetDefaultCmdResult(returncode=0, output=report, error=warnings)

    result = artifacts.GenerateCpeReport(self.chroot, self.sysroot,
                                         self.output_dir)

    self.assertEqual(self.result_file, result.report)
    self.assertEqual(self.warnings_file, result.warnings)
    self.assertFileContents(self.result_file, report)
    self.assertFileContents(self.warnings_file, warnings)

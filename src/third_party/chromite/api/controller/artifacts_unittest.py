# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Artifacts operations."""

from __future__ import print_function

import mock
import os

from chromite.api.controller import artifacts
from chromite.api.gen.chromite.api import artifacts_pb2
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import vm_test_stages
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib
from chromite.service import artifacts as artifacts_svc


class BundleTestCase(cros_test_lib.MockTestCase):
  """Basic setup for all artifacts unittests."""

  def setUp(self):
    self.input_proto = artifacts_pb2.BundleRequest()
    self.input_proto.build_target.name = 'target'
    self.input_proto.output_dir = '/tmp/artifacts'
    self.output_proto = artifacts_pb2.BundleResponse()

    self.PatchObject(constants, 'SOURCE_ROOT', new='/cros')


class BundleTempDirTestCase(cros_test_lib.MockTempDirTestCase):
  """Basic setup for artifacts unittests that need a tempdir."""

  def _GetRequest(self, chroot=None, sysroot=None, build_target=None,
                  output_dir=None):
    """Helper to create a request message instance.

    Args:
      chroot (str): The chroot path.
      sysroot (str): The sysroot path.
      build_target (str): The build target name.
      output_dir (str): The output directory.
    """
    return artifacts_pb2.BundleRequest(
        sysroot={'path': sysroot, 'build_target': {'name': build_target}},
        chroot={'path': chroot}, output_dir=output_dir)

  def _GetResponse(self):
    return artifacts_pb2.BundleResponse()

  def setUp(self):
    self.output_dir = os.path.join(self.tempdir, 'artifacts')
    osutils.SafeMakedirs(self.output_dir)

    # Old style paths.
    self.old_sysroot_path = os.path.join(self.tempdir, 'cros', 'chroot',
                                         'build', 'target')
    self.old_sysroot = sysroot_lib.Sysroot(self.old_sysroot_path)
    osutils.SafeMakedirs(self.old_sysroot_path)

    # Old style proto.
    self.input_proto = artifacts_pb2.BundleRequest()
    self.input_proto.build_target.name = 'target'
    self.input_proto.output_dir = self.output_dir
    self.output_proto = artifacts_pb2.BundleResponse()

    source_root = os.path.join(self.tempdir, 'cros')
    self.PatchObject(constants, 'SOURCE_ROOT', new=source_root)

    # New style paths.
    self.chroot_path = os.path.join(self.tempdir, 'cros', 'chroot')
    self.sysroot_path = '/build/target'
    self.full_sysroot_path = os.path.join(self.chroot_path,
                                          self.sysroot_path.lstrip(os.sep))
    self.sysroot = sysroot_lib.Sysroot(self.full_sysroot_path)
    osutils.SafeMakedirs(self.full_sysroot_path)

    # New style proto.
    self.request = artifacts_pb2.BundleRequest()
    self.request.output_dir = self.output_dir
    self.request.chroot.path = self.chroot_path
    self.request.sysroot.path = self.sysroot_path
    self.response = artifacts_pb2.BundleResponse()


class BundleImageZipTest(BundleTestCase):
  """Unittests for BundleImageZip."""

  def testBundleImageZip(self):
    """BundleImageZip calls cbuildbot/commands with correct args."""
    build_image_zip = self.PatchObject(
        commands, 'BuildImageZip', return_value='image.zip')
    self.PatchObject(os.path, 'exists', return_value=True)
    artifacts.BundleImageZip(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/image.zip'])
    self.assertEqual(
        build_image_zip.call_args_list,
        [mock.call('/tmp/artifacts', '/cros/src/build/images/target/latest')])

  def testBundleImageZipNoImageDir(self):
    """BundleImageZip dies when image dir does not exist."""
    self.PatchObject(os.path, 'exists', return_value=False)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageZip(self.input_proto, self.output_proto)


class BundleAutotestFilesTest(BundleTempDirTestCase):
  """Unittests for BundleAutotestFiles."""

  def testBundleAutotestFilesLegacy(self):
    """BundleAutotestFiles calls service correctly with legacy args."""
    files = {
        artifacts_svc.ARCHIVE_CONTROL_FILES: '/tmp/artifacts/autotest-a.tar.gz',
        artifacts_svc.ARCHIVE_PACKAGES: '/tmp/artifacts/autotest-b.tar.gz',
    }
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles',
                             return_value=files)

    sysroot_patch = self.PatchObject(sysroot_lib, 'Sysroot',
                                     return_value=self.old_sysroot)
    artifacts.BundleAutotestFiles(self.input_proto, self.output_proto)

    # Verify the sysroot is being built out correctly.
    sysroot_patch.assert_called_with(self.old_sysroot_path)

    # Verify the arguments are being passed through.
    patch.assert_called_with(self.old_sysroot, self.output_dir)

    # Verify the output proto is being populated correctly.
    self.assertTrue(self.output_proto.artifacts)
    paths = [artifact.path for artifact in self.output_proto.artifacts]
    self.assertItemsEqual(files.values(), paths)

  def testBundleAutotestFiles(self):
    """BundleAutotestFiles calls service correctly."""
    files = {
        artifacts_svc.ARCHIVE_CONTROL_FILES: '/tmp/artifacts/autotest-a.tar.gz',
        artifacts_svc.ARCHIVE_PACKAGES: '/tmp/artifacts/autotest-b.tar.gz',
    }
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles',
                             return_value=files)

    sysroot_patch = self.PatchObject(sysroot_lib, 'Sysroot',
                                     return_value=self.sysroot)
    artifacts.BundleAutotestFiles(self.request, self.response)

    # Verify the sysroot is being built out correctly.
    sysroot_patch.assert_called_with(self.full_sysroot_path)

    # Verify the arguments are being passed through.
    patch.assert_called_with(self.sysroot, self.output_dir)

    # Verify the output proto is being populated correctly.
    self.assertTrue(self.response.artifacts)
    paths = [artifact.path for artifact in self.response.artifacts]
    self.assertItemsEqual(files.values(), paths)

  def testInvalidOutputDir(self):
    """Test invalid output directory argument."""
    request = self._GetRequest(chroot=self.chroot_path,
                               sysroot=self.sysroot_path)
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, response)

  def testInvalidSysroot(self):
    """Test no sysroot directory."""
    request = self._GetRequest(chroot=self.chroot_path,
                               output_dir=self.output_dir)
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, response)

  def testSysrootDoesNotExist(self):
    """Test dies when no sysroot does not exist."""
    request = self._GetRequest(chroot=self.chroot_path,
                               sysroot='/does/not/exist',
                               output_dir=self.output_dir)
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, response)


class BundleTastFilesTest(BundleTestCase):
  """Unittests for BundleTastFiles."""

  def testBundleTastFiles(self):
    """BundleTastFiles calls cbuildbot/commands with correct args."""
    build_tast_bundle_tarball = self.PatchObject(
        commands,
        'BuildTastBundleTarball',
        return_value='/tmp/artifacts/tast.tar.gz')
    artifacts.BundleTastFiles(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/tast.tar.gz'])
    self.assertEqual(build_tast_bundle_tarball.call_args_list, [
        mock.call('/cros', '/cros/chroot/build/target/build', '/tmp/artifacts')
    ])

  def testBundleTastFilesNoLogs(self):
    """BundleTasteFiles dies when no tast files found."""
    self.PatchObject(commands, 'BuildTastBundleTarball',
                     return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTastFiles(self.input_proto, self.output_proto)


class BundlePinnedGuestImagesTest(BundleTestCase):
  """Unittests for BundlePinnedGuestImages."""

  def testBundlePinnedGuestImages(self):
    """BundlePinnedGuestImages calls cbuildbot/commands with correct args."""
    build_pinned_guest_images_tarball = self.PatchObject(
        commands,
        'BuildPinnedGuestImagesTarball',
        return_value='pinned-guest-images.tar.gz')
    artifacts.BundlePinnedGuestImages(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/pinned-guest-images.tar.gz'])
    self.assertEqual(build_pinned_guest_images_tarball.call_args_list,
                     [mock.call('/cros', 'target', '/tmp/artifacts')])

  def testBundlePinnedGuestImagesNoLogs(self):
    """BundlePinnedGuestImages does not die when no pinned images found."""
    self.PatchObject(commands, 'BuildPinnedGuestImagesTarball',
                     return_value=None)
    artifacts.BundlePinnedGuestImages(self.input_proto, self.output_proto)
    self.assertFalse(self.output_proto.artifacts)


class BundleFirmwareTest(BundleTestCase):
  """Unittests for BundleFirmware."""

  def testBundleFirmware(self):
    """BundleFirmware calls cbuildbot/commands with correct args."""
    build_firmware_archive = self.PatchObject(
        commands, 'BuildFirmwareArchive', return_value='firmware.tar.gz')
    artifacts.BundleFirmware(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/firmware.tar.gz'])
    self.assertEqual(build_firmware_archive.call_args_list,
                     [mock.call('/cros', 'target', '/tmp/artifacts')])

  def testBundleFirmwareNoLogs(self):
    """BundleFirmware dies when no firmware found."""
    self.PatchObject(commands, 'BuildFirmwareArchive', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleFirmware(self.input_proto, self.output_proto)


class BundleEbuildLogsTest(BundleTestCase):
  """Unittests for BundleEbuildLogs."""

  def testBundleEbuildLogs(self):
    """BundleEbuildLogs calls cbuildbot/commands with correct args."""
    build_ebuild_logs_tarball = self.PatchObject(
        commands, 'BuildEbuildLogsTarball', return_value='ebuild-logs.tar.gz')
    artifacts.BundleEbuildLogs(self.input_proto, self.output_proto)
    self.assertEqual(
        [artifact.path for artifact in self.output_proto.artifacts],
        ['/tmp/artifacts/ebuild-logs.tar.gz'])
    self.assertEqual(
        build_ebuild_logs_tarball.call_args_list,
        [mock.call('/cros/chroot/build', 'target', '/tmp/artifacts')])

  def testBundleEbuildLogsNoLogs(self):
    """BundleEbuildLogs dies when no logs found."""
    self.PatchObject(commands, 'BuildEbuildLogsTarball', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleEbuildLogs(self.input_proto, self.output_proto)


class BundleTestUpdatePayloadsTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for BundleTestUpdatePayloads."""

  def setUp(self):
    self.source_root = os.path.join(self.tempdir, 'cros')
    osutils.SafeMakedirs(self.source_root)

    self.archive_root = os.path.join(self.tempdir, 'output')
    osutils.SafeMakedirs(self.archive_root)

    self.target = 'target'
    self.image_root = os.path.join(self.source_root,
                                   'src/build/images/target/latest')

    self.input_proto = artifacts_pb2.BundleRequest()
    self.input_proto.build_target.name = self.target
    self.input_proto.output_dir = self.archive_root
    self.output_proto = artifacts_pb2.BundleResponse()

    self.PatchObject(constants, 'SOURCE_ROOT', new=self.source_root)

    def MockGeneratePayloads(image_path, archive_dir, **kwargs):
      assert kwargs
      osutils.WriteFile(os.path.join(archive_dir, 'payload.bin'), image_path)

    self.generate_payloads = self.PatchObject(
        commands, 'GeneratePayloads', side_effect=MockGeneratePayloads)

    def MockGenerateQuickProvisionPayloads(image_path, archive_dir):
      osutils.WriteFile(os.path.join(archive_dir, 'payload-qp.bin'), image_path)

    self.generate_quick_provision_payloads = self.PatchObject(
        commands,
        'GenerateQuickProvisionPayloads',
        side_effect=MockGenerateQuickProvisionPayloads)

  def testBundleTestUpdatePayloads(self):
    """BundleTestUpdatePayloads calls cbuildbot/commands with correct args."""
    image_path = os.path.join(self.image_root, constants.BASE_IMAGE_BIN)
    osutils.WriteFile(image_path, 'image!', makedirs=True)

    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)

    actual = [
        os.path.relpath(artifact.path, self.archive_root)
        for artifact in self.output_proto.artifacts
    ]
    expected = ['payload.bin', 'payload-qp.bin']
    self.assertItemsEqual(actual, expected)

    actual = [
        os.path.relpath(path, self.archive_root)
        for path in osutils.DirectoryIterator(self.archive_root)
    ]
    self.assertItemsEqual(actual, expected)

    self.assertEqual(self.generate_payloads.call_args_list, [
        mock.call(image_path, mock.ANY, full=True, stateful=True, delta=True),
    ])

    self.assertEqual(self.generate_quick_provision_payloads.call_args_list,
                     [mock.call(image_path, mock.ANY)])

  def testBundleTestUpdatePayloadsNoImageDir(self):
    """BundleTestUpdatePayloads dies if no image dir is found."""
    # Intentionally do not write image directory.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)

  def testBundleTestUpdatePayloadsNoImage(self):
    """BundleTestUpdatePayloads dies if no usable image is found for target."""
    # Intentionally do not write image, but create the directory.
    osutils.SafeMakedirs(self.image_root)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto)


class BundleSimpleChromeArtifactsTest(cros_test_lib.MockTempDirTestCase):
  """BundleSimpleChromeArtifacts tests."""

  def setUp(self):
    self.chroot_dir = os.path.join(self.tempdir, 'chroot_dir')
    self.sysroot_path = '/sysroot'
    self.sysroot_dir = os.path.join(self.chroot_dir, 'sysroot')
    osutils.SafeMakedirs(self.sysroot_dir)
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

    self.does_not_exist = os.path.join(self.tempdir, 'does_not_exist')

  def _GetRequest(self, chroot=None, sysroot=None, build_target=None,
                  output_dir=None):
    """Helper to create a request message instance.

    Args:
      chroot (str): The chroot path.
      sysroot (str): The sysroot path.
      build_target (str): The build target name.
      output_dir (str): The output directory.
    """
    return artifacts_pb2.BundleRequest(
        sysroot={'path': sysroot, 'build_target': {'name': build_target}},
        chroot={'path': chroot}, output_dir=output_dir)

  def _GetResponse(self):
    return artifacts_pb2.BundleResponse()

  def testNoBuildTarget(self):
    """Test no build target fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               output_dir=self.output_dir)
    response = self._GetResponse()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response)

  def testNoSysroot(self):
    """Test no sysroot fails."""
    request = self._GetRequest(build_target='board', output_dir=self.output_dir)
    response = self._GetResponse()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response)

  def testSysrootDoesNotExist(self):
    """Test no sysroot fails."""
    request = self._GetRequest(build_target='board', output_dir=self.output_dir,
                               sysroot=self.does_not_exist)
    response = self._GetResponse()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response)

  def testNoOutputDir(self):
    """Test no output dir fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board')
    response = self._GetResponse()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response)

  def testOutputDirDoesNotExist(self):
    """Test no output dir fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board',
                               output_dir=self.does_not_exist)
    response = self._GetResponse()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response)

  def testOutputHandling(self):
    """Test response output."""
    files = ['file1', 'file2', 'file3']
    expected_files = [os.path.join(self.output_dir, f) for f in files]
    self.PatchObject(artifacts_svc, 'BundleSimpleChromeArtifacts',
                     return_value=expected_files)
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board', output_dir=self.output_dir)
    response = self._GetResponse()

    artifacts.BundleSimpleChromeArtifacts(request, response)

    self.assertTrue(response.artifacts)
    self.assertItemsEqual(expected_files, [a.path for a in response.artifacts])


class BundleVmFilesTest(cros_test_lib.MockTestCase):
  """BuildVmFiles tests."""

  def _GetInput(self, chroot=None, sysroot=None, test_results_dir=None,
                output_dir=None):
    """Helper to build out an input message instance.

    Args:
      chroot (str|None): The chroot path.
      sysroot (str|None): The sysroot path relative to the chroot.
      test_results_dir (str|None): The test results directory relative to the
        sysroot.
      output_dir (str|None): The directory where the results tarball should be
        saved.
    """
    return artifacts_pb2.BundleVmFilesRequest(
        chroot={'path': chroot}, sysroot={'path': sysroot},
        test_results_dir=test_results_dir, output_dir=output_dir,
    )

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return artifacts_pb2.BundleResponse()

  def testChrootMissing(self):
    """Test error handling for missing chroot."""
    in_proto = self._GetInput(sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir='/tmp/output')
    out_proto = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, out_proto)

  def testSysrootMissing(self):
    """Test error handling for missing sysroot."""
    in_proto = self._GetInput(chroot='/chroot/dir',
                              test_results_dir='/test/results',
                              output_dir='/tmp/output')
    out_proto = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, out_proto)

  def testTestResultsDirMissing(self):
    """Test error handling for missing test results directory."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              output_dir='/tmp/output')
    out_proto = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, out_proto)

  def testOutputDirMissing(self):
    """Test error handling for missing output directory."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results')
    out_proto = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, out_proto)

  def testValidCall(self):
    """Test image dir building."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir='/tmp/output')
    out_proto = self._GetOutput()
    expected_files = ['/tmp/output/f1.tar', '/tmp/output/f2.tar']
    patch = self.PatchObject(vm_test_stages, 'ArchiveVMFilesFromImageDir',
                             return_value=expected_files)

    artifacts.BundleVmFiles(in_proto, out_proto)

    patch.assert_called_with('/chroot/dir/build/board/test/results',
                             '/tmp/output')

    # Make sure we have artifacts, and that every artifact is an expected file.
    self.assertTrue(out_proto.artifacts)
    for artifact in out_proto.artifacts:
      self.assertIn(artifact.path, expected_files)
      expected_files.remove(artifact.path)

    # Make sure we've seen all of the expected files.
    self.assertFalse(expected_files)

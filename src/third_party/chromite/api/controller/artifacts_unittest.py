# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Artifacts operations."""

from __future__ import print_function

import collections
import os

import mock

from chromite.api import api_config
from chromite.api.controller import artifacts
from chromite.api.gen.chromite.api import artifacts_pb2
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.cbuildbot import commands
from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib
from chromite.service import artifacts as artifacts_svc


PinnedGuestImage = collections.namedtuple('PinnedGuestImage',
                                          ['filename', 'uri'])


class BundleRequestMixin(object):
  """Mixin to provide bundle request methods."""

  def EmptyRequest(self):
    return artifacts_pb2.BundleRequest()

  def BuildTargetRequest(self, build_target=None, output_dir=None, chroot=None):
    """Get a build target format request instance."""
    request = self.EmptyRequest()
    if build_target:
      request.build_target.name = build_target
    if output_dir:
      request.output_dir = output_dir
    if chroot:
      request.chroot.path = chroot

    return request

  def SysrootRequest(self,
                     sysroot=None,
                     build_target=None,
                     output_dir=None,
                     chroot=None):
    """Get a sysroot format request instance."""
    request = self.EmptyRequest()
    if sysroot:
      request.sysroot.path = sysroot
    if build_target:
      request.sysroot.build_target.name = build_target
    if output_dir:
      request.output_dir = output_dir
    if chroot:
      request.chroot.path = chroot

    return request


class BundleTestCase(cros_test_lib.MockTempDirTestCase,
                     api_config.ApiConfigMixin, BundleRequestMixin):
  """Basic setup for all artifacts unittests."""

  def setUp(self):
    self.output_dir = os.path.join(self.tempdir, 'artifacts')
    osutils.SafeMakedirs(self.output_dir)
    self.sysroot_path = '/build/target'
    self.sysroot = sysroot_lib.Sysroot(self.sysroot_path)
    self.chroot_path = os.path.join(self.tempdir, 'chroot')
    full_sysroot_path = os.path.join(self.chroot_path,
                                     self.sysroot_path.lstrip(os.sep))
    osutils.SafeMakedirs(full_sysroot_path)

    # All requests use same response type.
    self.response = artifacts_pb2.BundleResponse()

    # Build target request.
    self.target_request = self.BuildTargetRequest(
        build_target='target',
        output_dir=self.output_dir,
        chroot=self.chroot_path)

    # Sysroot request.
    self.sysroot_request = self.SysrootRequest(
        sysroot=self.sysroot_path,
        build_target='target',
        output_dir=self.output_dir,
        chroot=self.chroot_path)

    self.source_root = self.tempdir
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)


class BundleImageArchivesTest(BundleTestCase):
  """BundleImageArchives tests."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'ArchiveImages')
    artifacts.BundleImageArchives(self.target_request, self.response,
                                  self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'ArchiveImages')
    artifacts.BundleImageArchives(self.target_request, self.response,
                                  self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 2)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'path0.tar.xz'))
    self.assertEqual(self.response.artifacts[1].path,
                     os.path.join(self.output_dir, 'path1.tar.xz'))

  def testNoBuildTarget(self):
    """Test that no build target fails."""
    request = self.BuildTargetRequest(output_dir=self.tempdir)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageArchives(request, self.response, self.api_config)

  def testNoOutputDir(self):
    """Test no output dir fails."""
    request = self.BuildTargetRequest(build_target='board')
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageArchives(request, self.response, self.api_config)

  def testInvalidOutputDir(self):
    """Test invalid output dir fails."""
    request = self.BuildTargetRequest(
        build_target='board', output_dir=os.path.join(self.tempdir, 'DNE'))
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageArchives(request, self.response, self.api_config)

  def testOutputHandling(self):
    """Test the artifact output handling."""
    expected = [os.path.join(self.output_dir, f) for f in ('a', 'b', 'c')]
    self.PatchObject(artifacts_svc, 'ArchiveImages', return_value=expected)
    self.PatchObject(os.path, 'exists', return_value=True)

    artifacts.BundleImageArchives(self.target_request, self.response,
                                  self.api_config)

    self.assertCountEqual(expected, [a.path for a in self.response.artifacts])


class BundleImageZipTest(BundleTestCase):
  """Unittests for BundleImageZip."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(commands, 'BuildImageZip')
    artifacts.BundleImageZip(self.target_request, self.response,
                             self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(commands, 'BuildImageZip')
    artifacts.BundleImageZip(self.target_request, self.response,
                             self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'image.zip'))

  def testBundleImageZip(self):
    """BundleImageZip calls cbuildbot/commands with correct args."""
    bundle_image_zip = self.PatchObject(
        artifacts_svc, 'BundleImageZip', return_value='image.zip')
    self.PatchObject(os.path, 'exists', return_value=True)
    artifacts.BundleImageZip(self.target_request, self.response,
                             self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'image.zip')])

    latest = os.path.join(self.source_root, 'src/build/images/target/latest')
    self.assertEqual(
        bundle_image_zip.call_args_list,
        [mock.call(self.output_dir, latest)])

  def testBundleImageZipNoImageDir(self):
    """BundleImageZip dies when image dir does not exist."""
    self.PatchObject(os.path, 'exists', return_value=False)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleImageZip(self.target_request, self.response,
                               self.api_config)


class BundleAutotestFilesTest(BundleTestCase):
  """Unittests for BundleAutotestFiles."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles')
    artifacts.BundleAutotestFiles(self.target_request, self.response,
                                  self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles')
    artifacts.BundleAutotestFiles(self.target_request, self.response,
                                  self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'autotest-a.tar.gz'))

  def testBundleAutotestFilesLegacy(self):
    """BundleAutotestFiles calls service correctly with legacy args."""
    files = {
        artifacts_svc.ARCHIVE_CONTROL_FILES: '/tmp/artifacts/autotest-a.tar.gz',
        artifacts_svc.ARCHIVE_PACKAGES: '/tmp/artifacts/autotest-b.tar.gz',
    }
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles',
                             return_value=files)

    artifacts.BundleAutotestFiles(self.target_request, self.response,
                                  self.api_config)

    # Verify the arguments are being passed through.
    patch.assert_called_with(mock.ANY, self.sysroot, self.output_dir)

    # Verify the output proto is being populated correctly.
    self.assertTrue(self.response.artifacts)
    paths = [artifact.path for artifact in self.response.artifacts]
    self.assertCountEqual(list(files.values()), paths)

  def testBundleAutotestFiles(self):
    """BundleAutotestFiles calls service correctly."""
    files = {
        artifacts_svc.ARCHIVE_CONTROL_FILES: '/tmp/artifacts/autotest-a.tar.gz',
        artifacts_svc.ARCHIVE_PACKAGES: '/tmp/artifacts/autotest-b.tar.gz',
    }
    patch = self.PatchObject(artifacts_svc, 'BundleAutotestFiles',
                             return_value=files)

    artifacts.BundleAutotestFiles(self.sysroot_request, self.response,
                                  self.api_config)

    # Verify the arguments are being passed through.
    patch.assert_called_with(mock.ANY, self.sysroot, self.output_dir)

    # Verify the output proto is being populated correctly.
    self.assertTrue(self.response.artifacts)
    paths = [artifact.path for artifact in self.response.artifacts]
    self.assertCountEqual(list(files.values()), paths)

  def testInvalidOutputDir(self):
    """Test invalid output directory argument."""
    request = self.SysrootRequest(chroot=self.chroot_path,
                                  sysroot=self.sysroot_path)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, self.response, self.api_config)

  def testInvalidSysroot(self):
    """Test no sysroot directory."""
    request = self.SysrootRequest(chroot=self.chroot_path,
                                  output_dir=self.output_dir)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, self.response, self.api_config)

  def testSysrootDoesNotExist(self):
    """Test dies when no sysroot does not exist."""
    request = self.SysrootRequest(chroot=self.chroot_path,
                                  sysroot='/does/not/exist',
                                  output_dir=self.output_dir)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleAutotestFiles(request, self.response, self.api_config)


class BundleTastFilesTest(BundleTestCase):
  """Unittests for BundleTastFiles."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleTastFiles')
    artifacts.BundleTastFiles(self.target_request, self.response,
                              self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleTastFiles')
    artifacts.BundleTastFiles(self.target_request, self.response,
                              self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'tast_bundles.tar.gz'))

  def testBundleTastFilesNoLogs(self):
    """BundleTasteFiles dies when no tast files found."""
    self.PatchObject(commands, 'BuildTastBundleTarball',
                     return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTastFiles(self.target_request, self.response,
                                self.api_config)

  def testBundleTastFilesLegacy(self):
    """BundleTastFiles handles legacy args correctly."""
    buildroot = self.tempdir
    chroot_dir = os.path.join(buildroot, 'chroot')
    sysroot_path = os.path.join(chroot_dir, 'build', 'board')
    output_dir = os.path.join(self.tempdir, 'results')
    osutils.SafeMakedirs(sysroot_path)
    osutils.SafeMakedirs(output_dir)

    chroot = chroot_lib.Chroot(chroot_dir)
    sysroot = sysroot_lib.Sysroot('/build/board')

    expected_archive = os.path.join(output_dir, artifacts_svc.TAST_BUNDLE_NAME)
    # Patch the service being called.
    bundle_patch = self.PatchObject(artifacts_svc, 'BundleTastFiles',
                                    return_value=expected_archive)
    self.PatchObject(constants, 'SOURCE_ROOT', new=buildroot)

    request = artifacts_pb2.BundleRequest(build_target={'name': 'board'},
                                          output_dir=output_dir)
    artifacts.BundleTastFiles(request, self.response, self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [expected_archive])
    bundle_patch.assert_called_once_with(chroot, sysroot, output_dir)

  def testBundleTastFiles(self):
    """BundleTastFiles calls service correctly."""
    chroot = chroot_lib.Chroot(self.chroot_path,
                               env={'FEATURES': 'separatedebug'})

    expected_archive = os.path.join(self.output_dir,
                                    artifacts_svc.TAST_BUNDLE_NAME)
    # Patch the service being called.
    bundle_patch = self.PatchObject(artifacts_svc, 'BundleTastFiles',
                                    return_value=expected_archive)

    artifacts.BundleTastFiles(self.sysroot_request, self.response,
                              self.api_config)

    # Make sure the artifact got recorded successfully.
    self.assertTrue(self.response.artifacts)
    self.assertEqual(expected_archive, self.response.artifacts[0].path)
    # Make sure the service got called correctly.
    bundle_patch.assert_called_once_with(chroot, self.sysroot, self.output_dir)


class BundlePinnedGuestImagesTest(BundleTestCase):
  """Unittests for BundlePinnedGuestImages."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(commands, 'BuildPinnedGuestImagesTarball')
    artifacts.BundlePinnedGuestImages(self.target_request, self.response,
                                      self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(commands, 'BuildPinnedGuestImagesTarball')
    artifacts.BundlePinnedGuestImages(self.target_request, self.response,
                                      self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir,
                                  'pinned-guest-images.tar.gz'))

  def testBundlePinnedGuestImages(self):
    """BundlePinnedGuestImages calls cbuildbot/commands with correct args."""
    build_pinned_guest_images_tarball = self.PatchObject(
        commands,
        'BuildPinnedGuestImagesTarball',
        return_value='pinned-guest-images.tar.gz')
    artifacts.BundlePinnedGuestImages(self.target_request, self.response,
                                      self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'pinned-guest-images.tar.gz')])
    self.assertEqual(build_pinned_guest_images_tarball.call_args_list,
                     [mock.call(self.source_root, 'target', self.output_dir)])

  def testBundlePinnedGuestImagesNoLogs(self):
    """BundlePinnedGuestImages does not die when no pinned images found."""
    self.PatchObject(commands, 'BuildPinnedGuestImagesTarball',
                     return_value=None)
    artifacts.BundlePinnedGuestImages(self.target_request, self.response,
                                      self.api_config)
    self.assertFalse(self.response.artifacts)


class FetchPinnedGuestImagesTest(cros_test_lib.MockTempDirTestCase,
                                 api_config.ApiConfigMixin, BundleRequestMixin):
  """Unittests for FetchPinnedGuestImages."""

  def setUp(self):
    self.build_target = 'board'
    self.chroot_dir = os.path.join(self.tempdir, 'chroot_dir')
    self.sysroot_path = '/sysroot'
    self.sysroot_dir = os.path.join(self.chroot_dir, 'sysroot')
    osutils.SafeMakedirs(self.sysroot_dir)

    self.input_request = artifacts_pb2.PinnedGuestImageUriRequest(
        sysroot={'path': self.sysroot_path,
                 'build_target': {'name': self.build_target}},
        chroot={'path': self.chroot_dir})

    self.response = artifacts_pb2.PinnedGuestImageUriResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'FetchPinnedGuestImages')
    artifacts.FetchPinnedGuestImages(self.input_request, self.response,
                                     self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'FetchPinnedGuestImages')
    artifacts.FetchPinnedGuestImages(self.input_request, self.response,
                                     self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.pinned_images), 1)
    self.assertEqual(self.response.pinned_images[0].filename,
                     'pinned_file.tar.gz')
    self.assertEqual(self.response.pinned_images[0].uri,
                     'https://testuri.com')

  def testFetchPinnedGuestImages(self):
    """FetchPinnedGuestImages calls service with correct args."""
    pins = []
    pins.append(PinnedGuestImage(
        filename='my_pinned_file.tar.gz', uri='https://the_testuri.com'))
    self.PatchObject(artifacts_svc, 'FetchPinnedGuestImages',
                     return_value=pins)
    artifacts.FetchPinnedGuestImages(self.input_request, self.response,
                                     self.api_config)
    self.assertEqual(len(self.response.pinned_images), 1)
    self.assertEqual(self.response.pinned_images[0].filename,
                     'my_pinned_file.tar.gz')
    self.assertEqual(self.response.pinned_images[0].uri,
                     'https://the_testuri.com')


class BundleFirmwareTest(BundleTestCase):
  """Unittests for BundleFirmware."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleTastFiles')
    artifacts.BundleFirmware(self.sysroot_request, self.response,
                             self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleTastFiles')
    artifacts.BundleFirmware(self.sysroot_request, self.response,
                             self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'firmware.tar.gz'))

  def testBundleFirmware(self):
    """BundleFirmware calls cbuildbot/commands with correct args."""
    self.PatchObject(
        artifacts_svc,
        'BuildFirmwareArchive',
        return_value=os.path.join(self.output_dir, 'firmware.tar.gz'))

    artifacts.BundleFirmware(self.sysroot_request, self.response,
                             self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'firmware.tar.gz')])

  def testBundleFirmwareNoLogs(self):
    """BundleFirmware dies when no firmware found."""
    self.PatchObject(commands, 'BuildFirmwareArchive', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleFirmware(self.sysroot_request, self.response,
                               self.api_config)


class BundleEbuildLogsTest(BundleTestCase):
  """Unittests for BundleEbuildLogs."""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(commands, 'BuildEbuildLogsTarball')
    artifacts.BundleEbuildLogs(self.target_request, self.response,
                               self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(commands, 'BuildEbuildLogsTarball')
    artifacts.BundleEbuildLogs(self.target_request, self.response,
                               self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'ebuild-logs.tar.gz'))

  def testBundleEbuildLogs(self):
    """BundleEbuildLogs calls cbuildbot/commands with correct args."""
    bundle_ebuild_logs_tarball = self.PatchObject(
        artifacts_svc, 'BundleEBuildLogsTarball',
        return_value='ebuild-logs.tar.gz')
    artifacts.BundleEbuildLogs(self.sysroot_request, self.response,
                               self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'ebuild-logs.tar.gz')])
    self.assertEqual(
        bundle_ebuild_logs_tarball.call_args_list,
        [mock.call(mock.ANY, self.sysroot, self.output_dir)])

  def testBundleEBuildLogsOldProto(self):
    bundle_ebuild_logs_tarball = self.PatchObject(
        artifacts_svc, 'BundleEBuildLogsTarball',
        return_value='ebuild-logs.tar.gz')

    artifacts.BundleEbuildLogs(self.target_request, self.response,
                               self.api_config)

    self.assertEqual(
        bundle_ebuild_logs_tarball.call_args_list,
        [mock.call(mock.ANY, self.sysroot, self.output_dir)])

  def testBundleEbuildLogsNoLogs(self):
    """BundleEbuildLogs dies when no logs found."""
    self.PatchObject(commands, 'BuildEbuildLogsTarball', return_value=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleEbuildLogs(self.sysroot_request, self.response,
                                 self.api_config)


class BundleChromeOSConfigTest(BundleTestCase):
  """Unittests for BundleChromeOSConfig"""

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleChromeOSConfig')
    artifacts.BundleChromeOSConfig(self.target_request, self.response,
                                   self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleChromeOSConfig')
    artifacts.BundleChromeOSConfig(self.target_request, self.response,
                                   self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'config.yaml'))

  def testBundleChromeOSConfigCallWithSysroot(self):
    """Call with a request that sets sysroot."""
    bundle_chromeos_config = self.PatchObject(
        artifacts_svc, 'BundleChromeOSConfig', return_value='config.yaml')
    artifacts.BundleChromeOSConfig(self.sysroot_request, self.response,
                                   self.api_config)
    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'config.yaml')])

    self.assertEqual(bundle_chromeos_config.call_args_list,
                     [mock.call(mock.ANY, self.sysroot, self.output_dir)])

  def testBundleChromeOSConfigCallWithBuildTarget(self):
    """Call with a request that sets build_target."""
    bundle_chromeos_config = self.PatchObject(
        artifacts_svc, 'BundleChromeOSConfig', return_value='config.yaml')
    artifacts.BundleChromeOSConfig(self.target_request, self.response,
                                   self.api_config)

    self.assertEqual(
        [artifact.path for artifact in self.response.artifacts],
        [os.path.join(self.output_dir, 'config.yaml')])

    self.assertEqual(bundle_chromeos_config.call_args_list,
                     [mock.call(mock.ANY, self.sysroot, self.output_dir)])

  def testBundleChromeOSConfigNoConfigFound(self):
    """An error is raised if the config payload isn't found."""
    self.PatchObject(artifacts_svc, 'BundleChromeOSConfig', return_value=None)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleChromeOSConfig(self.sysroot_request, self.response,
                                     self.api_config)


class BundleTestUpdatePayloadsTest(cros_test_lib.MockTempDirTestCase,
                                   api_config.ApiConfigMixin):
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

    def MockPayloads(image_path, archive_dir):
      osutils.WriteFile(os.path.join(archive_dir, 'payload1.bin'), image_path)
      osutils.WriteFile(os.path.join(archive_dir, 'payload2.bin'), image_path)
      return [os.path.join(archive_dir, 'payload1.bin'),
              os.path.join(archive_dir, 'payload2.bin')]

    self.bundle_patch = self.PatchObject(
        artifacts_svc, 'BundleTestUpdatePayloads', side_effect=MockPayloads)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleTestUpdatePayloads')
    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto,
                                       self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleTestUpdatePayloads')
    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto,
                                       self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.output_proto.artifacts), 1)
    self.assertEqual(self.output_proto.artifacts[0].path,
                     os.path.join(self.archive_root, 'payload1.bin'))

  def testBundleTestUpdatePayloads(self):
    """BundleTestUpdatePayloads calls cbuildbot/commands with correct args."""
    image_path = os.path.join(self.image_root, constants.BASE_IMAGE_BIN)
    osutils.WriteFile(image_path, 'image!', makedirs=True)

    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto,
                                       self.api_config)

    actual = [
        os.path.relpath(artifact.path, self.archive_root)
        for artifact in self.output_proto.artifacts
    ]
    expected = ['payload1.bin', 'payload2.bin']
    self.assertCountEqual(actual, expected)

    actual = [
        os.path.relpath(path, self.archive_root)
        for path in osutils.DirectoryIterator(self.archive_root)
    ]
    self.assertCountEqual(actual, expected)

  def testBundleTestUpdatePayloadsNoImageDir(self):
    """BundleTestUpdatePayloads dies if no image dir is found."""
    # Intentionally do not write image directory.
    artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto,
                                       self.api_config)
    self.assertFalse(self.output_proto.artifacts)

  def testBundleTestUpdatePayloadsNoImage(self):
    """BundleTestUpdatePayloads dies if no usable image is found for target."""
    # Intentionally do not write image, but create the directory.
    osutils.SafeMakedirs(self.image_root)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleTestUpdatePayloads(self.input_proto, self.output_proto,
                                         self.api_config)


class BundleSimpleChromeArtifactsTest(cros_test_lib.MockTempDirTestCase,
                                      api_config.ApiConfigMixin):
  """BundleSimpleChromeArtifacts tests."""

  def setUp(self):
    self.chroot_dir = os.path.join(self.tempdir, 'chroot_dir')
    self.sysroot_path = '/sysroot'
    self.sysroot_dir = os.path.join(self.chroot_dir, 'sysroot')
    osutils.SafeMakedirs(self.sysroot_dir)
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)

    self.does_not_exist = os.path.join(self.tempdir, 'does_not_exist')

    self.response = artifacts_pb2.BundleResponse()

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

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleSimpleChromeArtifacts')
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board', output_dir=self.output_dir)
    artifacts.BundleSimpleChromeArtifacts(request, self.response,
                                          self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleSimpleChromeArtifacts')
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board', output_dir=self.output_dir)
    artifacts.BundleSimpleChromeArtifacts(request, self.response,
                                          self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'simple_chrome.txt'))

  def testNoBuildTarget(self):
    """Test no build target fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               output_dir=self.output_dir)
    response = self.response
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

  def testNoSysroot(self):
    """Test no sysroot fails."""
    request = self._GetRequest(build_target='board', output_dir=self.output_dir)
    response = self.response
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

  def testSysrootDoesNotExist(self):
    """Test no sysroot fails."""
    request = self._GetRequest(build_target='board', output_dir=self.output_dir,
                               sysroot=self.does_not_exist)
    response = self.response
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

  def testNoOutputDir(self):
    """Test no output dir fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board')
    response = self.response
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

  def testOutputDirDoesNotExist(self):
    """Test no output dir fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board',
                               output_dir=self.does_not_exist)
    response = self.response
    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

  def testOutputHandling(self):
    """Test response output."""
    files = ['file1', 'file2', 'file3']
    expected_files = [os.path.join(self.output_dir, f) for f in files]
    self.PatchObject(artifacts_svc, 'BundleSimpleChromeArtifacts',
                     return_value=expected_files)
    request = self._GetRequest(chroot=self.chroot_dir,
                               sysroot=self.sysroot_path,
                               build_target='board', output_dir=self.output_dir)
    response = self.response

    artifacts.BundleSimpleChromeArtifacts(request, response, self.api_config)

    self.assertTrue(response.artifacts)
    self.assertCountEqual(expected_files, [a.path for a in response.artifacts])


class BundleVmFilesTest(cros_test_lib.MockTempDirTestCase,
                        api_config.ApiConfigMixin):
  """BuildVmFiles tests."""

  def setUp(self):
    self.output_dir = os.path.join(self.tempdir, 'output')
    osutils.SafeMakedirs(self.output_dir)

    self.response = artifacts_pb2.BundleResponse()

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

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc, 'BundleVmFiles')
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir=self.output_dir)
    artifacts.BundleVmFiles(in_proto, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'BundleVmFiles')
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir=self.output_dir)
    artifacts.BundleVmFiles(in_proto, self.response, self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'f1.tar'))

  def testChrootMissing(self):
    """Test error handling for missing chroot."""
    in_proto = self._GetInput(sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir=self.output_dir)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, self.response, self.api_config)

  def testTestResultsDirMissing(self):
    """Test error handling for missing test results directory."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              output_dir=self.output_dir)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, self.response, self.api_config)

  def testOutputDirMissing(self):
    """Test error handling for missing output directory."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, self.response, self.api_config)

  def testOutputDirDoesNotExist(self):
    """Test error handling for output directory that does not exist."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              output_dir=os.path.join(self.tempdir, 'dne'),
                              test_results_dir='/test/results')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.BundleVmFiles(in_proto, self.response, self.api_config)

  def testValidCall(self):
    """Test image dir building."""
    in_proto = self._GetInput(chroot='/chroot/dir', sysroot='/build/board',
                              test_results_dir='/test/results',
                              output_dir=self.output_dir)

    expected_files = ['/tmp/output/f1.tar', '/tmp/output/f2.tar']
    patch = self.PatchObject(artifacts_svc, 'BundleVmFiles',
                             return_value=expected_files)

    artifacts.BundleVmFiles(in_proto, self.response, self.api_config)

    patch.assert_called_with(mock.ANY, '/test/results', self.output_dir)

    # Make sure we have artifacts, and that every artifact is an expected file.
    self.assertTrue(self.response.artifacts)
    for artifact in self.response.artifacts:
      self.assertIn(artifact.path, expected_files)
      expected_files.remove(artifact.path)

    # Make sure we've seen all of the expected files.
    self.assertFalse(expected_files)



class BundleAFDOGenerationArtifactsTestCase(
    cros_test_lib.MockTempDirTestCase, api_config.ApiConfigMixin):
  """Unittests for BundleAFDOGenerationArtifacts."""

  @staticmethod
  def mock_die(message, *args):
    raise cros_build_lib.DieSystemExit(message % args)

  def setUp(self):
    self.chroot_dir = os.path.join(self.tempdir, 'chroot_dir')
    osutils.SafeMakedirs(self.chroot_dir)
    temp_dir = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(temp_dir)
    self.output_dir = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_dir)
    self.chrome_root = os.path.join(self.tempdir, 'chrome_root')
    osutils.SafeMakedirs(self.chrome_root)
    self.build_target = 'board'
    self.valid_artifact_type = toolchain_pb2.ORDERFILE
    self.invalid_artifact_type = toolchain_pb2.NONE_TYPE
    self.does_not_exist = os.path.join(self.tempdir, 'does_not_exist')
    self.PatchObject(cros_build_lib, 'Die', new=self.mock_die)

    self.response = artifacts_pb2.BundleResponse()

  def _GetRequest(self, chroot=None, build_target=None, chrome_root=None,
                  output_dir=None, artifact_type=None):
    """Helper to create a request message instance.

    Args:
      chroot (str): The chroot path.
      build_target (str): The build target name.
      chrome_root (str): The path to Chrome root.
      output_dir (str): The output directory.
      artifact_type (artifacts_pb2.AFDOArtifactType):
      The type of the artifact.
    """
    return artifacts_pb2.BundleChromeAFDORequest(
        chroot={'path': chroot, 'chrome_dir': chrome_root},
        build_target={'name': build_target},
        output_dir=output_dir,
        artifact_type=artifact_type,
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(artifacts_svc,
                             'BundleAFDOGenerationArtifacts')
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir,
                               artifact_type=self.valid_artifact_type)
    artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                            self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc,
                             'BundleAFDOGenerationArtifacts')
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir,
                               artifact_type=self.valid_artifact_type)
    artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                            self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 1)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.output_dir, 'artifact1'))

  def testNoBuildTarget(self):
    """Test no build target fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir,
                               artifact_type=self.valid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertEqual('build_target.name is required.',
                     str(context.exception))

  def testNoChromeRoot(self):
    """Test no chrome root fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               output_dir=self.output_dir,
                               artifact_type=self.valid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertEqual('chroot.chrome_dir path does not exist: ',
                     str(context.exception))

  def testNoOutputDir(self):
    """Test no output dir fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               artifact_type=self.valid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertEqual('output_dir is required.',
                     str(context.exception))

  def testOutputDirDoesNotExist(self):
    """Test output directory not existing fails."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.does_not_exist,
                               artifact_type=self.valid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertEqual(
        'output_dir path does not exist: %s' % self.does_not_exist,
        str(context.exception))

  def testNoArtifactType(self):
    """Test no artifact type."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertIn('artifact_type (0) must be in',
                  str(context.exception))

  def testWrongArtifactType(self):
    """Test passing wrong artifact type."""
    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir,
                               artifact_type=self.invalid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                              self.api_config)
    self.assertIn('artifact_type (%d) must be in' % self.invalid_artifact_type,
                  str(context.exception))

  def testOutputHandlingOnOrderfile(self,
                                    artifact_type=toolchain_pb2.ORDERFILE):
    """Test response output for orderfile."""
    files = ['artifact1', 'artifact2', 'artifact3']
    expected_files = [os.path.join(self.output_dir, f) for f in files]
    self.PatchObject(artifacts_svc, 'BundleAFDOGenerationArtifacts',
                     return_value=expected_files)

    request = self._GetRequest(chroot=self.chroot_dir,
                               build_target=self.build_target,
                               chrome_root=self.chrome_root,
                               output_dir=self.output_dir,
                               artifact_type=artifact_type)

    artifacts.BundleAFDOGenerationArtifacts(request, self.response,
                                            self.api_config)

    self.assertTrue(self.response.artifacts)
    self.assertCountEqual(expected_files,
                          [a.path for a in self.response.artifacts])

  def testOutputHandlingOnAFDO(self):
    """Test response output for AFDO."""
    self.testOutputHandlingOnOrderfile(
        artifact_type=toolchain_pb2.BENCHMARK_AFDO)


class ExportCpeReportTest(cros_test_lib.MockTempDirTestCase,
                          api_config.ApiConfigMixin):
  """ExportCpeReport tests."""

  def setUp(self):
    self.response = artifacts_pb2.BundleResponse()

  def testValidateOnly(self):
    """Sanity check validate only calls don't execute."""
    patch = self.PatchObject(artifacts_svc, 'GenerateCpeReport')

    request = artifacts_pb2.BundleRequest()
    request.build_target.name = 'board'
    request.output_dir = self.tempdir

    artifacts.ExportCpeReport(request, self.response, self.validate_only_config)

    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(artifacts_svc, 'GenerateCpeReport')

    request = artifacts_pb2.BundleRequest()
    request.build_target.name = 'board'
    request.output_dir = self.tempdir

    artifacts.ExportCpeReport(request, self.response, self.mock_call_config)

    patch.assert_not_called()
    self.assertEqual(len(self.response.artifacts), 2)
    self.assertEqual(self.response.artifacts[0].path,
                     os.path.join(self.tempdir, 'cpe_report.txt'))
    self.assertEqual(self.response.artifacts[1].path,
                     os.path.join(self.tempdir, 'cpe_warnings.txt'))

  def testNoBuildTarget(self):
    request = artifacts_pb2.BundleRequest()
    request.output_dir = self.tempdir

    with self.assertRaises(cros_build_lib.DieSystemExit):
      artifacts.ExportCpeReport(request, self.response, self.api_config)

  def testSuccess(self):
    """Test success case."""
    expected = artifacts_svc.CpeResult(
        report='/output/report.json', warnings='/output/warnings.json')
    self.PatchObject(artifacts_svc, 'GenerateCpeReport', return_value=expected)

    request = artifacts_pb2.BundleRequest()
    request.build_target.name = 'board'
    request.output_dir = self.tempdir

    artifacts.ExportCpeReport(request, self.response, self.api_config)

    for artifact in self.response.artifacts:
      self.assertIn(artifact.path, [expected.report, expected.warnings])

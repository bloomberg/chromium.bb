# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for build_artifact module.

These unit tests take tarball from google storage locations to fully test
the artifact download process. Please make sure to set up your boto file.
"""

from __future__ import print_function

import itertools
import os
import random
import shutil
import tempfile

import mock

from chromite.lib import cros_test_lib
from chromite.lib.xbuddy import artifact_info
from chromite.lib.xbuddy import build_artifact
from chromite.lib.xbuddy import devserver_constants
from chromite.lib.xbuddy import downloader

pytestmark = cros_test_lib.pytestmark_inside_only


_VERSION = 'R80-12653.0.0-rc1'
_BUILDER = 'amd64-generic-full'
_TEST_GOLO_BUILD_ID = '%s/%s' % (_BUILDER, _VERSION)
_TEST_GOLO_ARCHIVE = 'gs://chromeos-image-archive/' + _TEST_GOLO_BUILD_ID
_TEST_NON_EXISTING_GOLO_BUILD_ID = 'amd64-generic-full/R26-no_such_build'
_TEST_NON_EXISTING_GOLO_ARCHIVE = (
    'gs://chromeos-image-archive/' + _TEST_NON_EXISTING_GOLO_BUILD_ID)

_TEST_GOLO_ARCHIVE_TEST_TARBALL_CONTENT = [
    'autotest/test_suites/control.youtube_mse_eme',
    'autotest/test_suites/control.wifi_stress',
    'autotest/test_suites/control.wifi_endtoend',
    'autotest/test_suites/control.chameleon_hdmi_stress',
    'autotest/test_suites/control.enroll_retainment',
    'autotest/test_suites/control.wifi_interop_static',
    'autotest/test_suites/control.gce-smoke',
    'autotest/test_suites/control.power_requirements',
    'autotest/test_suites/control.hardware_storagequal',
    'autotest/test_suites/control.bluetooth_qualification',
    'autotest/test_suites/control.gpu_hang',
    'autotest/test_suites/control.faft_lab',
    'autotest/test_suites/control.AFDO_record',
    'autotest/test_suites/control.faft_lv5',
    'autotest/test_suites/control.link_perf',
    'autotest/test_suites/control.dummy',
    'autotest/test_suites/control.fingerprint',
    'autotest/test_suites/control.cellular_pseudomodem',
    'autotest/test_suites/control.wifi_release',
    'autotest/test_suites/control.regression',
    'autotest/test_suites/control.check_setup_storage_qual',
    'autotest/test_suites/control.chameleon_audio_perbuild',
    'autotest/test_suites/control.brillo-bvt',
    'autotest/test_suites/control.usb-camera',
    'autotest/test_suites/control.wilco_bve',
    'autotest/test_suites/control.wifi_tdls_cast',
    'autotest/test_suites/control.cellular_ota_verizon',
    'autotest/test_suites/control.chrome-informational',
    'autotest/test_suites/control.security',
    'autotest/test_suites/control.faft_pd',
    'autotest/test_suites/control.video',
    'autotest/test_suites/control.ent-wificell',
    'autotest/test_suites/control.wifi_interop_wpa2',
    'autotest/test_suites/control.chameleon_audio_unstable',
    'autotest/test_suites/control.paygen_au_beta',
    'autotest/test_suites/control.bluetooth_sanity',
    'autotest/test_suites/control.vmtest_informational4',
    'autotest/test_suites/control.faft_ec',
    'autotest/test_suites/control.kernel_daily_benchmarks',
    'autotest/test_suites/control.wifi_perf',
    'autotest/test_suites/control.faft_ec_au_2',
    'autotest/test_suites/control.bluetooth_standalone',
    'autotest/test_suites/control.moblab_quick',
    'autotest/test_suites/control.telemetry_unit_server',
    'autotest/test_suites/control.faft_smoke',
    'autotest/test_suites/control.vmtest_informational1',
    'autotest/test_suites/control.audio',
    'autotest/test_suites/control.faft_lv3',
    'autotest/test_suites/control.touch',
    'autotest/test_suites/control.cellular_endtoend',
    'autotest/test_suites/control.faft_setup',
    'autotest/test_suites/control.faft_bios_tot',
    'autotest/test_suites/control.graphics_per-build',
    'autotest/test_suites/control.cellular_au_nightly',
    'autotest/test_suites/control.vmtest_informational2',
    'autotest/test_suites/control.faft_normal',
    'autotest/test_suites/control.crosbolt_perf_weekly',
    'autotest/test_suites/control.sb65-presubmit',
    'autotest/test_suites/control.faft_flashrom',
    'autotest/test_suites/control.paygen_au_canary',
    'autotest/test_suites/control.bluetooth_stress',
    'autotest/test_suites/control.bvt-cq',
    'autotest/test_suites/control.au-perbuild',
    'autotest/test_suites/control.brillo-experimental',
    'autotest/test_suites/control.moblab_storage_qual',
    'autotest/test_suites/control.power_measurement_wrapper',
    'autotest/test_suites/control.deqp',
    'autotest/test_suites/control.kiosk_longevity',
    'autotest/test_suites/control.faft_ec_au_1',
    'autotest/test_suites/control.faft_lv4',
    'autotest/test_suites/control.hwqual',
    'autotest/test_suites/control.power_loadtest_fast',
    'autotest/test_suites/control.bluetooth',
    'autotest/test_suites/control.bvt-tast-cq',
    'autotest/test_suites/control.cellular_ota',
    'autotest/test_suites/control.powerplay',
    'autotest/test_suites/control.power_loadtest_1hour',
    'autotest/test_suites/control.dummyflake',
    'autotest/test_suites/control.bluetooth_flaky',
    'autotest/test_suites/control.wifichaos',
    'autotest/test_suites/control.faft_ec_au_3',
    'autotest/test_suites/control.graphics_system',
    'autotest/test_suites/control.provision',
    'autotest/test_suites/control.cups_weekly',
    'autotest/test_suites/control.bvt-tast-chrome-pfq',
    'autotest/test_suites/control.bluestreak-pre-cq',
    'autotest/test_suites/control.bvt-perbuild',
    'autotest/test_suites/control.au_fsi',
    'autotest/test_suites/control.faft_cr50_prepvt',
    'autotest/test_suites/control.bvt-tast-android-pfq',
    'autotest/test_suites/control.hardware_memoryqual',
    'autotest/test_suites/control.sanity',
    'autotest/test_suites/control.brillo-postsubmit',
    'autotest/test_suites/control.hardware_storagequal_cq',
    'autotest/test_suites/control.hardware_memoryqual_quick',
    'autotest/test_suites/control.faft_infra',
    'autotest/test_suites/control.wifi_matfunc_bcm4356',
    'autotest/test_suites/control.gts',
    'autotest/test_suites/control.dummy_server_nossp',
    'autotest/test_suites/control.faft_lv2',
    'autotest/test_suites/control.faft_bios_au_2',
    'autotest/test_suites/control.faft_lv1',
    'autotest/test_suites/control.av_webcam',
    'autotest/test_suites/control.power_sanity',
    'autotest/test_suites/control.wificell-pre-cq',
    'autotest/test_suites/control.hotrod',
    'autotest/test_suites/control.skylab_staging_test',
    'autotest/test_suites/control.perfalerts',
    'autotest/test_suites/control.bvt-installer',
    'autotest/test_suites/control.graphics_browser',
    'autotest/test_suites/control.network_nightly',
    'autotest/test_suites/control.moblab',
    'autotest/test_suites/control.ent-nightly',
    'autotest/test_suites/control.kernel_weekly_regression',
    'autotest/test_suites/control.crosbolt_perf_perbuild',
    'autotest/test_suites/control.wifi_matfunc',
    'autotest/test_suites/control.hardware_storagequal_quick',
    'autotest/test_suites/control.push_to_prod',
    'autotest/test_suites/control.gce-sanity',
    'autotest/test_suites/control.suite_attr_wrapper',
    'autotest/test_suites/control.platform_test_nightly',
    'autotest/test_suites/control.faft_bios_au_1',
    'autotest/test_suites/control.faft_cr50_experimental',
    'autotest/test_suites/control.kernel_per-build_regression',
    'autotest/test_suites/control.app-compat',
    'autotest/test_suites/control.jailed_build',
    'autotest/test_suites/control.bvt-arc',
    'autotest/test_suites/control.wifi_flaky',
    'autotest/test_suites/suite_to_control_file_map',
    'autotest/test_suites/control.test_that_wrapper',
    'autotest/test_suites/control.experimental',
    'autotest/test_suites/control.ent-perbuild',
    'autotest/test_suites/control.wifi_update_router',
    'autotest/test_suites/control.faft_cr50_pvt',
    'autotest/test_suites/control.stress',
    'autotest/test_suites/control.cellular_ota_sprint',
    'autotest/test_suites/control.bvt-inline',
    'autotest/test_suites/control.brillo-smoke',
    'autotest/test_suites/control.faft_ec3po',
    'autotest/test_suites/control.video_image_comparison_chameleon',
    'autotest/test_suites/control.smoke',
    'autotest/test_suites/control.moblab-cts-mini',
    'autotest/test_suites/control.cellular_mbim_compliance',
    'autotest/test_suites/control.kernel_daily_regression',
    'autotest/test_suites/control.cellular_au',
    'autotest/test_suites/control.faft_bios',
    'autotest/test_suites/control.kernel_per-build_benchmarks',
    'autotest/test_suites/control.usb_detect_stress',
    'autotest/test_suites/control.crosbolt_perf_nightly',
    'autotest/test_suites/control.tablet_mode',
    'autotest/test_suites/control.bluetooth_wifi_coex',
    'autotest/test_suites/control.youtube_page',
    'autotest/test_suites/control.faft_dev',
    'autotest/test_suites/control.storagequal',
    'autotest/test_suites/control.wifi_atten_perf',
    'autotest/test_suites/control.cr50_stress',
    'autotest/test_suites/control.cellular_ota_tmobile',
    'autotest/test_suites/control.wifi_interop',
    'autotest/test_suites/control.arc-unit-test',
    'autotest/test_suites/control.bluestreak-partners',
    'autotest/test_suites/control.chameleon_hdmi_unstable',
    'autotest/test_suites/control.graphics',
    'autotest/test_suites/control.cros_test_platform',
    'autotest/test_suites/control.faft_cr50_tot',
    'autotest/test_suites/control.network_ui',
    'autotest/test_suites/control.brillo-presubmit',
    'autotest/test_suites/control.graphics_per-week',
    'autotest/test_suites/control.usb_detect',
    'autotest/test_suites/control.partners',
    'autotest/test_suites/control.faft_ec_tot',
    'autotest/test_suites/control.check_setup_cts_N',
    'autotest/test_suites/control.tendo_experimental',
    'autotest/test_suites/control.power_idle',
    'autotest/test_suites/control.vmtest_informational3',
    'autotest/test_suites/control.faft_bios_au_3',
    'autotest/test_suites/control.dummy_server',
    'autotest/test_suites/control.cts_P',
    'autotest/test_suites/control.bluetooth_e2e',
    'autotest/test_suites/control.power_daily',
    'autotest/test_suites/control.cellular_ota_att',
    'autotest/test_suites/dependency_info',
    'autotest/test_suites/control.power_loadtest',
    'autotest/test_suites/control.chameleon_hdmi_perbuild',
    'autotest/test_suites/control.cellular_modem_repair',
    'autotest/test_suites/control.faft_wilco',
    'autotest/test_suites/control.graphics_per-day',
    'autotest/test_suites/control.manual_platform_suite',
    'autotest/test_suites/control.hardware_storagequal_temp',
    'autotest/test_suites/control.paygen_au_dev',
    'autotest/test_suites/control.cros-av-analysis',
    'autotest/test_suites/control.bvt-tast-informational',
    'autotest/test_suites/control.brillo-audio',
    'autotest/test_suites/control.cts_N',
    'autotest/test_suites/control.faft_bios_ec3po',
    'autotest/test_suites/control.power_build',
    'autotest/test_suites/control.toolchain-tests',
    'autotest/test_suites/control.security_weekly',
    'autotest/test_suites/control.wifi_lucidsleep',
    'autotest/test_suites/control.telemetry_unit',
    'autotest/test_suites/control.paygen_au_stable',
]

_TEST_GOLO_ARCHIVE_IMAGE_ZIPFILE_CONTENT = [
    'image.zip',
    'unpack_partitions.sh',
    'chromiumos_base_image.bin-package-sizes.json',
    'config.txt',
    'pack_partitions.sh',
    'umount_image.sh',
    'id_rsa.pub',
    'boot.config',
    'partition_script.sh',
    'boot_images/vmlinuz-4.14.151-13598-g75d9ce1f04f5',
    'boot_images/vmlinuz',
    'mount_image.sh',
    'id_rsa',
    'license_credits.html',
    'boot.desc',
]


class BuildArtifactTest(cros_test_lib.MockTestCase):
  """Test different BuildArtifact operations."""

  def setUp(self):
    self.work_dir = tempfile.mkdtemp('build_artifact_unittest')
    self.dl = downloader.GoogleStorageDownloader(self.work_dir,
                                                 _TEST_GOLO_ARCHIVE,
                                                 _TEST_GOLO_BUILD_ID)

  def tearDown(self):
    shutil.rmtree(self.work_dir)

  def _CheckMarker(self, marker_file, installed_files):
    with open(os.path.join(self.work_dir, marker_file)) as f:
      self.assertEqual(installed_files, f.read().splitlines())

  def testBundledArtifactTypes(self):
    """Tests that all known bundled artifacts are either zip or tar files."""
    known_names = ['zip', '.tgz', '.tar', 'tar.bz2', 'tar.xz', 'tar.gz']
    # pylint: disable=dict-values-not-iterating
    for d in itertools.chain(*build_artifact.chromeos_artifact_map.values()):
      if issubclass(d, build_artifact.BundledArtifact):
        self.assertTrue(any(d.ARTIFACT_NAME.endswith(name)
                            for name in known_names))

  @cros_test_lib.NetworkTest()
  def testProcessBuildArtifact(self):
    """Processes a real tarball from GSUtil and stages it."""
    artifact = build_artifact.Artifact(
        build_artifact.TEST_SUITES_FILE, self.work_dir, _VERSION)

    artifact.Process(self.dl, False)
    self.assertEqual(
        artifact.installed_files,
        [os.path.join(self.work_dir, build_artifact.TEST_SUITES_FILE)])
    self.assertExists(os.path.join(self.work_dir,
                                   build_artifact.TEST_SUITES_FILE))
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testProcessTarball(self):
    """Downloads a real tarball and untars it."""
    artifact = build_artifact.BundledArtifact(
        build_artifact.TEST_SUITES_FILE, self.work_dir, _VERSION)
    artifact.Process(self.dl, False)
    expected_installed_files = [
        os.path.join(self.work_dir, filename)
        for filename in ([build_artifact.TEST_SUITES_FILE] +
                         _TEST_GOLO_ARCHIVE_TEST_TARBALL_CONTENT)]
    self.assertEqual(artifact.installed_files, expected_installed_files)
    self.assertTrue(os.path.isdir(os.path.join(
        self.work_dir, 'autotest', 'test_suites')))
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testProcessTarballWithFile(self):
    """Downloads a real tarball and only untars one file from it."""
    file_to_download = 'autotest/test_suites/control.provision'
    artifact = build_artifact.BundledArtifact(
        build_artifact.TEST_SUITES_FILE, self.work_dir, _VERSION,
        files_to_extract=[file_to_download])
    expected_installed_files = [
        os.path.join(self.work_dir, filename)
        for filename in [build_artifact.TEST_SUITES_FILE] + [file_to_download]]
    artifact.Process(self.dl, False)
    self.assertEqual(artifact.installed_files, expected_installed_files)
    self.assertExists(os.path.join(self.work_dir,
                                   file_to_download))
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @mock.patch.object(build_artifact.AutotestTarball, '_Extract')
  @mock.patch.object(build_artifact.AutotestTarball, '_UpdateName')
  @mock.patch.object(downloader.GoogleStorageDownloader, 'Fetch')
  @mock.patch.object(downloader.GoogleStorageDownloader, 'Wait')
  def testDownloadAutotest(self, wait_mock, fetch_mock,
                           update_name_mock, extract_mock):
    """Downloads a real autotest tarball for test."""
    artifact = build_artifact.AutotestTarball(
        build_artifact.AUTOTEST_FILE, self.work_dir, _VERSION,
        files_to_extract=None, exclude=['autotest/test_suites'])

    install_dir = self.work_dir
    artifact.staging_dir = install_dir

    rc = self.StartPatcher(cros_test_lib.RunCommandMock())
    rc.SetDefaultCmdResult()
    artifact.Process(self.dl, True)

    self.assertFalse(artifact.installed_files)
    self.assertTrue(os.path.isdir(
        os.path.join(self.work_dir, 'autotest', 'packages')))
    self.assertFalse(os.path.getsize(
        os.path.join(self.work_dir, artifact.marker_name)))

    wait_mock.assert_called_with('autotest.tar', False, None, 1)
    fetch_mock.assert_called_with('autotest.tar', install_dir + '/')
    update_name_mock.assert_called()
    extract_mock.assert_called()
    rc.assertCommandContains(
        ['autotest/utils/packager.py', '--action=upload', '--repository',
         os.path.join(install_dir, 'autotest/packages'), '--all'],
        cwd=install_dir)

  @cros_test_lib.NetworkTest()
  def testStatefulPayloadArtifact(self):
    """Tests downloading the stateful payload."""
    factory = build_artifact.ChromeOSArtifactFactory(
        self.work_dir, [artifact_info.STATEFUL_PAYLOAD], [], _VERSION)
    artifacts = factory.RequiredArtifacts()
    self.assertEqual(len(artifacts), 1)

    artifact = artifacts[0]
    expected_installed_files = [os.path.join(self.work_dir,
                                             devserver_constants.STATEFUL_FILE)]
    artifact.Process(self.dl, False)
    self.assertEqual(artifact.installed_files, expected_installed_files)
    for f in expected_installed_files:
      self.assertExists(f)
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testAUFullPayloadArtifact(self):
    """Tests downloading the full update payload payload."""
    factory = build_artifact.ChromeOSArtifactFactory(
        self.work_dir, [artifact_info.FULL_PAYLOAD], [], _VERSION)
    artifacts = factory.RequiredArtifacts()
    self.assertEqual(len(artifacts), 1)

    artifact = artifacts[0]
    expected_installed_files = [
        os.path.join(self.work_dir, f)
        for f in (devserver_constants.UPDATE_FILE,
                  devserver_constants.UPDATE_METADATA_FILE)]
    artifact.Process(self.dl, False)
    self.assertEqual(artifact.installed_files, expected_installed_files)
    for f in expected_installed_files:
      self.assertExists(f)
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testAUDeltaPayloadArtifact(self):
    """Tests downloading the delta update payload payload."""
    factory = build_artifact.ChromeOSArtifactFactory(
        self.work_dir, [artifact_info.DELTA_PAYLOAD], [], _VERSION)
    artifacts = factory.RequiredArtifacts()
    self.assertEqual(len(artifacts), 1)

    artifact = artifacts[0]
    expected_installed_files = [
        os.path.join(self.work_dir, build_artifact.AU_NTON_DIR, f)
        for f in (devserver_constants.UPDATE_FILE,
                  devserver_constants.UPDATE_METADATA_FILE)]
    artifact.Process(self.dl, False)
    self.assertEqual(artifact.installed_files, expected_installed_files)
    for f in expected_installed_files:
      self.assertExists(f)
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testImageUnzip(self):
    """Downloads and stages a zip file and extracts a test image."""
    files_to_extract = ['chromiumos_test_image.bin']
    artifact = build_artifact.BundledArtifact(
        build_artifact.IMAGE_FILE, self.work_dir, _VERSION,
        files_to_extract=files_to_extract)
    expected_installed_files = [
        os.path.join(self.work_dir, filename)
        for filename in [build_artifact.IMAGE_FILE] + files_to_extract]
    artifact.Process(self.dl, False)
    self.assertEqual(expected_installed_files, artifact.installed_files)
    for f in expected_installed_files:
      self.assertExists(f)
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testImageUnzipWithExcludes(self):
    """Downloads and stages a zip file while excluding all large files."""
    artifact = build_artifact.BundledArtifact(
        build_artifact.IMAGE_FILE, self.work_dir, _VERSION, exclude=['*.bin'])
    artifact.Process(self.dl, False)

    expected_installed_files = [
        os.path.join(self.work_dir, filename)
        for filename in _TEST_GOLO_ARCHIVE_IMAGE_ZIPFILE_CONTENT]
    self.assertEqual(expected_installed_files, artifact.installed_files)
    self.assertNotExists(os.path.join(self.work_dir,
                                      'chromiumos_test_image.bin'))
    self._CheckMarker(artifact.marker_name, artifact.installed_files)

  @cros_test_lib.NetworkTest()
  def testArtifactFactory(self):
    """Tests that BuildArtifact works for both named and file artifacts."""
    name_artifact = 'test_suites' # This file is in every real GS dir.
    file_artifact = 'metadata.json' # This file is in every real GS dir.
    factory = build_artifact.ChromeOSArtifactFactory(
        self.work_dir, [name_artifact], [file_artifact], _VERSION)
    artifacts = factory.RequiredArtifacts()
    self.assertEqual(len(artifacts), 2)
    expected_installed_files_0 = [
        os.path.join(self.work_dir, filename) for filename
        in ([build_artifact.TEST_SUITES_FILE] +
            _TEST_GOLO_ARCHIVE_TEST_TARBALL_CONTENT)]
    expected_installed_files_1 = [os.path.join(self.work_dir, file_artifact)]
    artifacts[0].Process(self.dl, False)
    artifacts[1].Process(self.dl, False)
    self.assertEqual(artifacts[0].installed_files, expected_installed_files_0)
    self.assertEqual(artifacts[1].installed_files, expected_installed_files_1)
    # Test suites directory exists.
    self.assertExists(os.path.join(self.work_dir, 'autotest', 'test_suites'))
    # File artifact was staged.
    self.assertExists(os.path.join(self.work_dir, file_artifact))
    self._CheckMarker(artifacts[0].marker_name, artifacts[0].installed_files)
    self._CheckMarker(artifacts[1].marker_name, artifacts[1].installed_files)

  def testProcessBuildArtifactWithException(self):
    """Test processing a non-existing artifact from GSUtil."""
    artifact = build_artifact.Artifact(
        build_artifact.TEST_SUITES_FILE, self.work_dir, _VERSION)
    try:
      dl = downloader.GoogleStorageDownloader(self.work_dir,
                                              _TEST_NON_EXISTING_GOLO_ARCHIVE,
                                              _TEST_NON_EXISTING_GOLO_BUILD_ID)
      artifact.Process(dl, False)
    except Exception as e:
      expected_exception = e
    saved_exception = artifact.GetException()
    # saved_exception has traceback info - strip it.
    self.assertEqual(str(expected_exception),
                     str(saved_exception).split('\n')[0])

  @cros_test_lib.NetworkTest()
  def testArtifactStaged(self):
    """Tests the artifact staging verification logic."""
    artifact = build_artifact.BundledArtifact(
        build_artifact.TEST_SUITES_FILE, self.work_dir, _VERSION)
    expected_installed_files = [
        os.path.join(self.work_dir, filename)
        for filename in ([build_artifact.TEST_SUITES_FILE] +
                         _TEST_GOLO_ARCHIVE_TEST_TARBALL_CONTENT)]
    artifact.Process(self.dl, False)

    # Check that it works when all files are there.
    self.assertTrue(artifact.ArtifactStaged())

    # Remove an arbitrary file among the ones staged, ensure the check fails
    # and that the marker files is removed.
    os.remove(random.choice(expected_installed_files))
    self.assertExists(os.path.join(self.work_dir, artifact.marker_name))
    self.assertFalse(artifact.ArtifactStaged())
    self.assertNotExists(os.path.join(self.work_dir, artifact.marker_name))

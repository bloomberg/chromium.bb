# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for downloader module."""

from __future__ import print_function

import os
import shutil
import tempfile

import mock

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib.xbuddy import build_artifact
from chromite.lib.xbuddy import downloader

pytestmark = cros_test_lib.pytestmark_inside_only


# pylint: disable=protected-access,no-value-for-parameter
class DownloaderTest(cros_test_lib.TestCase):
  """Downloader Unittests."""

  def setUp(self):
    self._work_dir = tempfile.mkdtemp('downloader-test')
    self.board = 'kevin-full'
    self.build = 'R81-12829.0.0-rc1'
    self.archive_dir = (
        'chromeos-image-archive/%s/%s' % (self.board, self.build))
    self.archive_url = 'gs://' + self.archive_dir
    self.local_path = ('/local/path/%s/%s' % (self.board, self.build))
    self.payload = 'chromeos_{build}_{board}_dev.bin'.format(
        build=self.build, board=self.board.replace('-', '_'))
    self.downloaded = []
    self.dest_dir = os.path.join(self._work_dir, self.board, self.build) + '/'

  def tearDown(self):
    shutil.rmtree(self._work_dir, ignore_errors=True)

  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsSerially')
  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsInBackground')
  def _SimpleDownloadOfTestSuites(self, downloader_instance, bg_mock,
                                  serial_mock):
    """Helper to verify test_suites are downloaded correctly.

    Args:
      downloader_instance: Downloader object to test with.
      bg_mock: background download method mock.
      serial_mock: serial download method mock.
    """
    factory = build_artifact.ChromeOSArtifactFactory(
        downloader_instance.GetBuildDir(), ['test_suites'],
        None, downloader_instance.GetBuild())

    downloader_instance.Download(factory)
    # Sanity check the timestamp file exists.
    install_dir = os.path.join(self._work_dir, self.board, self.build)
    self.assertExists(
        os.path.join(install_dir, downloader.Downloader._TIMESTAMP_FILENAME))
    serial_mock.assert_called()
    bg_mock.assert_called()

  def testSimpleDownloadOfTestSuitesFromGS(self):
    """Basic test_suites test.

    Verifies that if we request the test_suites from Google Storage, it gets
    downloaded and the autotest tarball is attempted in the background.
    """
    self._SimpleDownloadOfTestSuites(
        downloader.GoogleStorageDownloader(
            self._work_dir, self.archive_url,
            downloader.GoogleStorageDownloader.GetBuildIdFromArchiveURL(
                self.archive_url)))

  def testSimpleDownloadOfTestSuitesFromLocal(self):
    """Basic test_suites test.

    Verifies that if we request the test_suites from a local path, it gets
    downloaded and the autotest tarball is attempted in the background.
    """
    self._SimpleDownloadOfTestSuites(
        downloader.LocalDownloader(self._work_dir, self.local_path))

  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsSerially')
  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsInBackground')
  def _DownloadSymbolsHelper(self, downloader_instance, bg_mock, serial_mock):
    """Basic symbols download."""
    factory = build_artifact.ChromeOSArtifactFactory(
        downloader_instance.GetBuildDir(), ['symbols'],
        None, downloader_instance.GetBuild())

    # Should not get called but mocking so that we know it wasn't called.
    downloader_instance.Download(factory)
    serial_mock.assert_called()
    bg_mock.assert_not_called()

  def testDownloadSymbolsFromGS(self):
    """Basic symbols download from Google Storage."""
    self._DownloadSymbolsHelper(
        downloader.GoogleStorageDownloader(
            self._work_dir, self.archive_url,
            downloader.GoogleStorageDownloader.GetBuildIdFromArchiveURL(
                self.archive_url)))

  def testDownloadSymbolsFromLocal(self):
    """Basic symbols download from a Local Path."""
    self._DownloadSymbolsHelper(
        downloader.LocalDownloader(self._work_dir, self.local_path))

  def _FetchMock(self, remote_name, local_path):
    """Mock for GoogleStorageDownloader.Fetch.

    Create empty files in the download directory.

    Args:
      remote_name: Remote name of the file to fetch.
      local_path: Local path to the folder to store fetched file.

    Returns:
      The path to fetched file.
    """
    for d in self.downloaded:
      osutils.Touch(d, makedirs=True)
    return os.path.join(remote_name, local_path)

  def _DownloadFullPayload(self):
    """Full payload download."""
    downloader_instance = downloader.GoogleStorageDownloader(
        self._work_dir, self.archive_url,
        downloader.GoogleStorageDownloader.GetBuildIdFromArchiveURL(
            self.archive_url))
    factory = build_artifact.ChromeOSArtifactFactory(
        downloader_instance.GetBuildDir(), ['full_payload'],
        None, downloader_instance.GetBuild())

    full_dev = os.path.join(self.dest_dir, self.payload)
    self.downloaded = [full_dev, full_dev + '.json']

    return downloader_instance, factory

  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsInBackground')
  def testFullPayloadDownload(self, bg_mock):
    """Test full payload download."""
    downloader_instance, factory = self._DownloadFullPayload()
    with mock.patch.object(downloader.GoogleStorageDownloader, 'Wait',
                           return_value=self.downloaded) as wait_mock:
      with mock.patch.object(downloader.GoogleStorageDownloader, 'Fetch',
                             side_effect=self._FetchMock) as fetch_mock:
        downloader_instance.Download(factory)
    # Sanity check the timestamp file exists.
    install_dir = os.path.join(self._work_dir, self.board, self.build)
    self.assertExists(
        os.path.join(install_dir, downloader.Downloader._TIMESTAMP_FILENAME))
    bg_mock.assert_not_called()
    wait_mock.assert_called()
    fetch_mock.assert_called_with(
        os.path.join(self.dest_dir, self.payload) + '.json', self.dest_dir)


  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsInBackground')
  def testAnonymousDownload(self, bg_mock):
    """Test anonymous download of full payload."""
    err_msg = ('ServiceException: 401 Anonymous caller does not have '
               'storage.objects.get access to ' + self.archive_dir + '.')
    gs_error = gs.GSCommandError(err_msg, cros_build_lib.CommandResult(
        error=err_msg))

    downloader_instance, factory = self._DownloadFullPayload()
    with mock.patch.object(downloader.GoogleStorageDownloader, 'Fetch',
                           side_effect=self._FetchMock) as fetch_mock:
      with mock.patch.object(gs.GSContext, 'GetGsNamesWithWait',
                             side_effect=gs_error) as gs_mock:
        downloader_instance.Download(factory)
        gs_mock.assert_called()

    # Sanity check the timestamp file exists.
    install_dir = os.path.join(self._work_dir, self.board, self.build)
    self.assertExists(
        os.path.join(install_dir, downloader.Downloader._TIMESTAMP_FILENAME))
    bg_mock.assert_not_called()
    fetch_mock.assert_called_with(self.payload + '.json', self.dest_dir)


class AndroidDownloaderTest(cros_test_lib.TestCase):
  """Android Downloader Unittests."""

  def setUp(self):
    self._work_dir = tempfile.mkdtemp('downloader-test')
    self.branch = 'release'
    self.target = 'shamu-userdebug'
    self.build_id = '123456'

  def tearDown(self):
    shutil.rmtree(self._work_dir, ignore_errors=True)

  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsSerially')
  @mock.patch.object(downloader.Downloader, '_DownloadArtifactsInBackground')
  def testDownloadFromAndroidBuildServer(self, bg_mock, serial_mock):
    """Basic test to check download from Android's build server works."""
    downloader_instance = downloader.AndroidBuildDownloader(
        self._work_dir, self.branch, self.build_id, self.target)
    factory = build_artifact.AndroidArtifactFactory(
        downloader_instance.GetBuildDir(), ['fastboot'],
        None, downloader_instance.GetBuild())

    downloader_instance.Download(factory)
    # Sanity check the timestamp file exists.
    self.assertExists(
        os.path.join(self._work_dir, self.branch, self.target, self.build_id,
                     downloader.Downloader._TIMESTAMP_FILENAME))
    serial_mock.assert_called()
    bg_mock.assert_not_called()

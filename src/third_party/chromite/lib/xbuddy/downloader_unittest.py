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

from chromite.lib import cros_test_lib
from chromite.lib.xbuddy import build_artifact
from chromite.lib.xbuddy import downloader


# pylint: disable=protected-access,no-value-for-parameter
class DownloaderTestBase(cros_test_lib.TestCase):
  """Downloader Unittests."""

  def setUp(self):
    self._work_dir = tempfile.mkdtemp('downloader-test')
    self.board = 'x86-mario-release'
    self.build = 'R17-1413.0.0-a1-b1346'
    self.archive_url = (
        'gs://chromeos-image-archive/%s/%s' % (self.board, self.build))
    self.local_path = ('/local/path/x86-mario-release/R17-1413.0.0-a1-b1346')

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


class AndroidDownloaderTestBase(cros_test_lib.TestCase):
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

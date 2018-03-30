# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import mock  # pylint: disable=import-error

from py_utils import cloud_storage
from telemetry.wpr import archive_info

from core import path_util
import fetch_benchmark_deps

def NormPaths(paths):
  return sorted([os.path.normcase(p) for p in paths.splitlines()])


class FetchBenchmarkDepsUnittest(unittest.TestCase):
  """The test guards fetch_benchmark_deps.

  It assumes the following telemetry APIs always success:
  telemetry.wpr.archive_info.WprArchiveInfo.DownloadArchivesIfNeeded
  py_utils.cloud_storage.GetFilesInDirectoryIfChanged
  """

  def testFetchWPRs(self):
    args = ['smoothness.top_25_smooth']
    with mock.patch.object(archive_info.WprArchiveInfo,
        'DownloadArchivesIfNeeded', autospec=True) as mock_download:
      with mock.patch('py_utils.cloud_storage'
                      '.GetFilesInDirectoryIfChanged') as mock_get:
        mock_download.return_value = True
        mock_get.GetFilesInDirectoryIfChanged.return_value = True
        fetch_benchmark_deps.main(args)
        self.assertEqual(
            # pylint: disable=protected-access
            os.path.normpath(mock_download.call_args[0][0]._file_path),
            os.path.join(path_util.GetPerfStorySetsDir(), 'data',
            'top_25_smooth.json'))
        # This benchmark doesn't use any static local files.
        self.assertFalse(mock_get.called)

  def testFetchServingDirs(self):
    args = ['media.desktop']
    with mock.patch.object(archive_info.WprArchiveInfo,
        'DownloadArchivesIfNeeded', autospec=True) as mock_download:
      with mock.patch('py_utils.cloud_storage'
                      '.GetFilesInDirectoryIfChanged') as mock_get:
        mock_download.return_value = True
        mock_get.GetFilesInDirectoryIfChanged.return_value = True
        fetch_benchmark_deps.main(args)
        # This benchmark doesn't use any archive files.
        self.assertFalse(mock_download.called)
        mock_get.assert_called_once_with(
            os.path.join(path_util.GetPerfStorySetsDir(), 'media_cases'),
            cloud_storage.PARTNER_BUCKET)

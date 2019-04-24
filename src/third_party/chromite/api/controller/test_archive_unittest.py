# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Archive tests."""

from __future__ import print_function

import os

from chromite.api.controller import test_archive as test_archive_controller
from chromite.api.gen import test_archive_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.service import test_archive as test_archive_service


class CreateHwTestArchivesTest(cros_test_lib.MockTempDirTestCase):
  """CreateHwTestArchives tests."""
  _SUCCESS_RETURN = {
      test_archive_service.ARCHIVE_CONTROL_FILES: '/path/to/control.tar',
      test_archive_service.ARCHIVE_PACKAGES: '/path/to/packages.tar',
      test_archive_service.ARCHIVE_SERVER_PACKAGES: '/path/to/server.tar',
      test_archive_service.ARCHIVE_TEST_SUITES: '/path/to/tests.tar',
  }

  def setUp(self):
    self.board = 'board'
    self.PatchObject(path_util, 'FromChrootPath',
                     side_effect=lambda x: os.path.join(self.tempdir, x))

    osutils.SafeMakedirs(os.path.join(self.tempdir, 'build', self.board))

  def testSuccess(self):
    """Successful call response building test."""
    self.PatchObject(test_archive_service, 'CreateHwTestArchives',
                     return_value=self._SUCCESS_RETURN)

    request = test_archive_pb2.CreateArchiveRequest()
    request.build_target.name = self.board
    request.output_directory = self.tempdir

    response = test_archive_pb2.CreateArchiveResponse()

    test_archive_controller.CreateHwTestArchives(request, response)

    expected_files = self._SUCCESS_RETURN.values()
    for result_file in response.files:
      self.assertIn(result_file.path, expected_files)
      expected_files.remove(result_file.path)
    # Make sure it was a perfect intersection.
    self.assertEqual([], expected_files)

  def testRequiredArguments(self):
    """Test argument validation."""
    request = test_archive_pb2.CreateArchiveRequest()
    response = test_archive_pb2.CreateArchiveResponse()

    # Missing everything.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_archive_controller.CreateHwTestArchives(request, response)

    request.build_target.name = self.board
    # Only missing output directory.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_archive_controller.CreateHwTestArchives(request, response)

    request.build_target.name = ''
    request.output_directory = self.tempdir
    # Only missing board.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_archive_controller.CreateHwTestArchives(request, response)

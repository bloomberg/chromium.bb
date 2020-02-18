# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the autotest_util module."""

from __future__ import print_function

import os

import mock

from chromite.lib import autotest_util
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.utils import matching


class BuildTarballTests(cros_test_lib.RunCommandTempDirTestCase):
  """Tests related to building tarball artifacts."""
  # pylint: disable=protected-access

  def setUp(self):
    self._buildroot = os.path.join(self.tempdir, 'buildroot')
    os.makedirs(self._buildroot)
    self._board = 'test-board'
    self.basedir = os.path.normpath(
        os.path.join(self._buildroot, 'chroot', 'build', self._board,
                     constants.AUTOTEST_BUILD_PATH, '..'))
    self.builder = autotest_util.AutotestTarballBuilder(self.basedir,
                                                        self.tempdir)

  def testBuildAutotestPackagesTarball(self):
    """Tests that generating the autotest packages tarball is correct."""
    tar_mock = self.PatchObject(self.builder, '_BuildTarball')
    tar_path = os.path.join(self.tempdir, self.builder._PACKAGES_ARCHIVE)

    self.builder.BuildAutotestPackagesTarball()

    tar_mock.assert_called_once_with(['autotest/packages'], tar_path,
                                     compressed=False)

  def testBuildAutotestTestSuitesTarball(self):
    """Tests that generating the autotest packages tarball is correct."""
    tar_mock = self.PatchObject(self.builder, '_BuildTarball')
    tar_path = os.path.join(self.tempdir, self.builder._TEST_SUITES_ARCHIVE)

    self.builder.BuildAutotestTestSuitesTarball()

    tar_mock.assert_called_once_with(['autotest/test_suites'], tar_path)

  def testBuildAutotestControlFilesTarball(self):
    """Tests that generating the autotest control files tarball is correct."""
    control_file_list = ['autotest/client/site_tests/testA/control',
                         'autotest/server/site_tests/testB/control']
    tar_path = os.path.join(self.tempdir, self.builder._CONTROL_FILES_ARCHIVE)

    tar_mock = self.PatchObject(self.builder, '_BuildTarball')
    self.PatchObject(matching, 'FindFilesMatching',
                     return_value=control_file_list)

    self.builder.BuildAutotestControlFilesTarball()

    tar_mock.assert_called_once_with(control_file_list, tar_path,
                                     compressed=False)

  def testBuildAutotestServerPackageTarball(self):
    """Tests that generating the autotest server package tarball is correct."""
    file_list = ['autotest/server/site_tests/testA/control',
                 'autotest/server/site_tests/testB/control']
    tar_path = os.path.join(self.tempdir, self.builder._SERVER_PACKAGE_ARCHIVE)

    expected_files = list(file_list)
    ssp_files = list()

    # Touch chroot Tast paths so they'll be included in the tar command.
    for p in self.builder._TAST_SSP_CHROOT_FILES:
      path = '%s%s' % (self.basedir, p)
      osutils.Touch(path, makedirs=True)
      expected_files.append(path)
      ssp_files.append(path)

    # Skip touching the source Tast files so we can verify they're not included
    # in the tar command.
    for p in self.builder._TAST_SSP_SOURCE_FILES:
      path = '%s%s' % (self.basedir, p)
      ssp_files.append(path)

    tar_mock = self.PatchObject(self.builder, '_BuildTarball')
    self.PatchObject(self.builder, '_GetTastSspFiles', return_value=ssp_files)
    # Pass a copy of the file list so the code under test can't mutate it.
    self.PatchObject(matching, 'FindFilesMatching',
                     return_value=list(file_list))

    self.builder.BuildAutotestServerPackageTarball()

    tar_mock.assert_called_once_with(expected_files, tar_path,
                                     extra_args=mock.ANY, error_code_ok=True)

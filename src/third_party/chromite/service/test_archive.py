# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test archive functionality."""

from __future__ import print_function

import os

from chromite.lib import autotest_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import path_util


# Archive type constants.
ARCHIVE_CONTROL_FILES = 'control'
ARCHIVE_PACKAGES = 'packages'
ARCHIVE_SERVER_PACKAGES = 'server_packages'
ARCHIVE_TEST_SUITES = 'test_suites'


class Error(Exception):
  """Base error class for the module."""


class ArchiveBaseDirNotFound(Error):
  """Raised when the archive base directory does not exist.

  This error most likely indicates the board was not built.
  """


def CreateHwTestArchives(board, output_directory):
  """Create the Hardware Test archives.

  Args:
    board (str): The name of the board whose artifacts are being created.
    output_directory (str): The path were the completed archives should be put.

  Returns:
    dict - The paths of the files created in |output_directory| by their type.
  """
  # archive_basedir is the base directory where the archive commands are run.
  # We want the folder containing the board's autotest folder.
  archive_basedir = os.path.join('/build', board, constants.AUTOTEST_BUILD_PATH,
                                 '..')
  archive_basedir = os.path.normpath(archive_basedir)
  if not cros_build_lib.IsInsideChroot():
    # We built it out as if we're inside the chroot, convert when not inside.
    archive_basedir = path_util.FromChrootPath(archive_basedir)

  if not os.path.exists(archive_basedir):
    raise ArchiveBaseDirNotFound(
        'Archive base directory does not exist: %s' % archive_basedir)

  builder = autotest_util.AutotestTarballBuilder(archive_basedir,
                                                 output_directory)
  return {
      ARCHIVE_CONTROL_FILES: builder.BuildAutotestControlFilesTarball(),
      ARCHIVE_TEST_SUITES: builder.BuildAutotestTestSuitesTarball(),
      ARCHIVE_PACKAGES: builder.BuildAutotestPackagesTarball(),
      ARCHIVE_SERVER_PACKAGES: builder.BuildAutotestServerPackageTarball(),
  }

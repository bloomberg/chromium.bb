# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Artifacts service.

This service houses the high level business logic for all created artifacts.
"""

from __future__ import print_function

import glob
import os

from chromite.lib import autotest_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


# Archive type constants.
ARCHIVE_CONTROL_FILES = 'control'
ARCHIVE_PACKAGES = 'packages'
ARCHIVE_SERVER_PACKAGES = 'server_packages'
ARCHIVE_TEST_SUITES = 'test_suites'


class Error(Exception):
  """Base module error."""


class ArchiveBaseDirNotFound(Error):
  """Raised when the archive base directory does not exist.

  This error most likely indicates the board was not built.
  """


class CrosGenerateSysrootError(Error):
  """Error when running CrosGenerateSysroot."""


class NoFilesException(Error):
  """When there are no files to archive."""


def BundleAutotestFiles(sysroot, output_directory):
  """Create the Autotest Hardware Test archives.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot whose artifacts are being
      archived.
    output_directory (str): The path were the completed archives should be put.

  Returns:
    dict - The paths of the files created in |output_directory| by their type.
  """
  assert sysroot.Exists()
  assert output_directory

  # archive_basedir is the base directory where the archive commands are run.
  # We want the folder containing the board's autotest folder.
  archive_basedir = os.path.join(sysroot.path, constants.AUTOTEST_BUILD_PATH)
  archive_basedir = os.path.dirname(archive_basedir)

  if not os.path.exists(archive_basedir):
    raise ArchiveBaseDirNotFound(
        'Archive base directory does not exist: %s' % archive_basedir)

  builder = autotest_util.AutotestTarballBuilder(archive_basedir,
                                                 output_directory)
  return {
      ARCHIVE_CONTROL_FILES: builder.BuildAutotestControlFilesTarball(),
      ARCHIVE_PACKAGES: builder.BuildAutotestPackagesTarball(),
      ARCHIVE_SERVER_PACKAGES: builder.BuildAutotestServerPackageTarball(),
      ARCHIVE_TEST_SUITES: builder.BuildAutotestTestSuitesTarball(),
  }


def BundleSimpleChromeArtifacts(chroot, sysroot, build_target, output_dir):
  """Gather all of the simple chrome artifacts.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    sysroot (sysroot_lib.Sysroot): The sysroot.
    build_target (build_target_util.BuildTarget): The sysroot's build target.
    output_dir (str): Where all result files should be stored.
  """
  files = []
  files.extend(CreateChromeRoot(chroot, build_target, output_dir))
  files.append(ArchiveChromeEbuildEnv(sysroot, output_dir))

  return files


def ArchiveChromeEbuildEnv(sysroot, output_dir):
  """Generate Chrome ebuild environment.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot where the original environment
      archive can be found.
    output_dir (str): Where the result should be stored.

  Returns:
    str: The path to the archive.

  Raises:
    NoFilesException: When the package cannot be found.
  """
  pkg_dir = os.path.join(sysroot.path, 'var', 'db', 'pkg')
  files = glob.glob(os.path.join(pkg_dir, constants.CHROME_CP) + '-*')
  if not files:
    raise NoFilesException('Failed to find package %s' % constants.CHROME_CP)

  if len(files) > 1:
    logging.warning('Expected one package for %s, found %d',
                    constants.CHROME_CP, len(files))

  chrome_dir = sorted(files)[-1]
  env_bzip = os.path.join(chrome_dir, 'environment.bz2')
  result_path = os.path.join(output_dir, constants.CHROME_ENV_TAR)
  with osutils.TempDir() as tempdir:
    # Convert from bzip2 to tar format.
    bzip2 = cros_build_lib.FindCompressor(cros_build_lib.COMP_BZIP2)
    tempdir_tar_path = os.path.join(tempdir, constants.CHROME_ENV_FILE)
    cros_build_lib.RunCommand([bzip2, '-d', env_bzip, '-c'],
                              log_stdout_to_file=tempdir_tar_path)

    cros_build_lib.CreateTarball(result_path, tempdir)

  return result_path


def CreateChromeRoot(chroot, build_target, output_dir):
  """Create the chrome sysroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot in which the sysroot should be built.
    build_target (build_target_util.BuildTarget): The build target.
    output_dir (str): The location outside the chroot where the files should be
      stored.

  Returns:
    list[str]: The list of created files.

  Raises:
    CrosGenerateSysrootError: When cros_generate_sysroot does not complete
      successfully.
  """
  chroot_args = chroot.GetEnterArgs()

  extra_env = {'USE': 'chrome_internal'}
  base_dir = os.path.join(chroot.path, 'tmp')
  with osutils.TempDir(base_dir=base_dir) as tempdir:
    in_chroot_path = os.path.relpath(tempdir, chroot.path)
    cmd = ['cros_generate_sysroot', '--out-dir', in_chroot_path, '--board',
           build_target.name, '--deps-only', '--package', constants.CHROME_CP]

    try:
      cros_build_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env,
                                chroot_args=chroot_args)
    except cros_build_lib.RunCommandError as e:
      raise CrosGenerateSysrootError(
          'Error encountered when running cros_generate_sysroot: %s' % e, e)

    files = []
    for path in osutils.DirectoryIterator(tempdir):
      if os.path.isfile(path):
        rel_path = os.path.relpath(path, tempdir)
        files.append(os.path.join(output_dir, rel_path))
    osutils.CopyDirContents(tempdir, output_dir, allow_nonempty=True)

    return files

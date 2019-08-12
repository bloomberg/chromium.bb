# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Image API is the entry point for image functionality."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import portage_util


PARALLEL_EMERGE_STATUS_FILE_NAME = 'status_file'


class Error(Exception):
  """Base module error."""


class InvalidArgumentError(Error):
  """Invalid argument values."""


class ImageToVmError(Error):
  """Error converting the image to a vm."""


class BuildConfig(object):
  """Value object to hold the build configuration options."""

  def __init__(self, builder_path=None, disk_layout=None,
               enable_rootfs_verification=True, replace=False, version=None,
               build_attempt=None, symlink=None):
    """Build config initialization.

    Args:
      builder_path (str): The value to which the builder path lsb key should be
        set, the build_name installed on DUT during hwtest.
      disk_layout (str): The disk layout type.
      enable_rootfs_verification (bool): Whether the rootfs verification is
        enabled.
      replace (bool): Whether to replace existing output if any exists.
      version (str): The version string to use for the image.
      build_attempt (int): The build_attempt number to pass to build_image.
      symlink (str): Symlink string.
    """
    self.builder_path = builder_path
    self.disk_layout = disk_layout
    self.enable_rootfs_verification = enable_rootfs_verification
    self.replace = replace
    self.version = version
    self.build_attempt = build_attempt
    self.symlink = symlink

  def GetArguments(self):
    """Get the build_image arguments for the configuration."""
    args = []

    if self.builder_path:
      args.extend(['--builder_path', self.builder_path])
    if self.disk_layout:
      args.extend(['--disk_layout', self.disk_layout])
    if not self.enable_rootfs_verification:
      args.append('--noenable_rootfs_verification')
    if self.replace:
      args.append('--replace')
    if self.version:
      args.extend(['--version', self.version])
    if self.build_attempt:
      args.extend(['--build_attempt', self.build_attempt])
    if self.symlink:
      args.extend(['--symlink', self.symlink])

    return args


class BuildResult(object):
  """Value object to report build image results."""

  def __init__(self, return_code, failed_packages):
    """Init method.

    Args:
      return_code (int): The build return code.
      failed_packages (list[str]): A list of failed packages as strings.
    """
    self.failed_packages = []
    for package in failed_packages or []:
      self.failed_packages.append(portage_util.SplitCPV(package, strict=False))

    # The return code should always be non-zero if there's any failed packages,
    # but it's cheap insurance, so check it.
    self.success = return_code == 0 and not self.failed_packages


def Build(board=None, images=None, config=None, extra_env=None):
  """Build an image.

  Args:
    board (str): The board name.
    images (list): The image types to build.
    config (BuildConfig): The build configuration options.
    extra_env (dict): Environment variables to set for build_image.

  Returns:
    BuildResult
  """
  board = board or cros_build_lib.GetDefaultBoard()
  if not board:
    raise InvalidArgumentError('board is required.')
  images = images or [constants.IMAGE_TYPE_BASE]
  config = config or BuildConfig()

  if cros_build_lib.IsInsideChroot():
    cmd = [os.path.join(constants.CROSUTILS_DIR, 'build_image')]
  else:
    cmd = ['./build_image']

  cmd.extend(['--board', board])
  cmd.extend(config.GetArguments())
  cmd.extend(images)

  extra_env_local = extra_env.copy() if extra_env else {}

  with osutils.TempDir() as tempdir:
    status_file = os.path.join(tempdir, PARALLEL_EMERGE_STATUS_FILE_NAME)
    extra_env_local[constants.PARALLEL_EMERGE_STATUS_FILE_ENVVAR] = status_file
    result = cros_build_lib.RunCommand(cmd, enter_chroot=True,
                                       error_code_ok=True,
                                       extra_env=extra_env_local)
    try:
      content = osutils.ReadFile(status_file).strip()
    except IOError:
      # No file means no packages.
      failed = None
    else:
      failed = content.split() if content else None

    return BuildResult(result.returncode, failed)


def CreateVm(board, is_test=False, chroot=None):
  """Create a VM from an image.

  Args:
    board (str): The board for which the VM is being created.
    is_test (bool): Whether it is a test image.
    chroot (chroot_lib.Chroot): The chroot where the image lives.

  Returns:
    str: Path to the created VM .bin file.
  """
  assert board
  cmd = ['./image_to_vm.sh', '--board', board]

  if is_test:
    cmd.append('--test_image')

  chroot_args = None
  if chroot and cros_build_lib.IsOutsideChroot():
    chroot_args = chroot.GetEnterArgs()

  result = cros_build_lib.RunCommand(cmd, error_code_ok=True,
                                     enter_chroot=True, chroot_args=chroot_args)

  if result.returncode:
    # Error running the command. Unfortunately we can't be much more helpful
    # than this right now.
    raise ImageToVmError('Unable to convert the image to a VM. '
                         'Consult the logs to determine the problem.')

  vm_path = os.path.join(image_lib.GetLatestImageLink(board),
                         constants.VM_IMAGE_BIN)
  return os.path.realpath(vm_path)


def Test(board, result_directory, image_dir=None):
  """Run tests on an already built image.

  Currently this is just running test_image.

  Args:
    board (str): The board name.
    result_directory (str): Root directory where the results should be stored
      relative to the chroot.
    image_dir (str): The path to the image. Uses the board's default image
      build path when not provided.

  Returns:
    bool - True if all tests passed, False otherwise.
  """
  if not board:
    raise InvalidArgumentError('Board is required.')
  if not result_directory:
    raise InvalidArgumentError('Result directory required.')

  if not image_dir:
    # We can build the path to the latest image directory.
    image_dir = image_lib.GetLatestImageLink(board, force_chroot=True)
  elif not cros_build_lib.IsInsideChroot() and os.path.exists(image_dir):
    # Outside chroot with outside chroot path--we need to convert it.
    image_dir = path_util.ToChrootPath(image_dir)

  cmd = [
      os.path.join(constants.CHROOT_SOURCE_ROOT, constants.CHROMITE_BIN_SUBDIR,
                   'test_image'),
      '--board', board,
      '--test_results_root', result_directory,
      image_dir,
  ]

  result = cros_build_lib.SudoRunCommand(cmd, enter_chroot=True,
                                         error_code_ok=True)

  return result.returncode == 0

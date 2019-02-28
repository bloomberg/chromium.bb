# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Operations to work with the SDK chroot."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_sdk_lib


class ChrootPaths(object):
  """Value object to hold common cros_sdk path arguments."""

  def __init__(self, cache_dir=None, chrome_root=None, chroot_path=None):
    """Chroot paths init.

    Args:
      cache_dir (str): Override the default cache directory.
      chrome_root (str): Set the chrome root directory to mount in the chroot.
      chroot_path (str): Set the path the chroot resides (or will be created).
    """
    self.cache_dir = cache_dir
    self.chrome_root = chrome_root
    self.chroot_path = chroot_path

  def GetArgList(self):
    """Get the list of the corresponding commandline arguments.

    Returns:
      list - The list of the corresponding command line arguments.
    """
    args = []

    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])

    if self.chrome_root:
      args.extend(['--chrome_root', self.chrome_root])

    if self.chroot_path:
      args.extend(['--chroot', self.chroot_path])

    return args


class CreateArguments(object):
  """Value object to handle the chroot creation arguments."""

  def __init__(self, replace=False, bootstrap=False, use_image=True,
               paths=None):
    """Create arguments init.

    Args:
      replace (bool): Whether an existing chroot should be deleted.
      bootstrap (bool): Whether to build the SDK from source.
      use_image (bool): Whether to mount the chroot on a loopback image or
        create it directly in a directory.
      paths (ChrootPaths): Path arguments.
    """
    self.replace = replace
    self.bootstrap = bootstrap
    self.use_image = use_image
    self.paths = paths or ChrootPaths()

  def GetArgList(self):
    """Get the list of the corresponding command line arguments.

    Returns:
      list - The list of the corresponding command line arguments.
    """
    args = []

    if self.replace:
      args.append('--replace')
    else:
      args.append('--create')

    if self.bootstrap:
      args.append('--bootstrap')

    if not self.use_image:
      args.append('--nouse-image')

    args.extend(self.paths.GetArgList())

    return args


class UpdateArguments(object):
  """Value object to handle the update arguments."""

  def __init__(self, build_source=False, toolchain_targets=None):
    """Update arguments init.

    Args:
      build_source (bool): Whether to build the source or use prebuilts.
      toolchain_targets (list): The list of build targets whose toolchains
        should be updated.
    """
    self.build_source = build_source
    self.toolchain_targets = toolchain_targets

  def GetArgList(self):
    """Get the list of the corresponding command line arguments.

    Returns:
      list - The list of the corresponding command line arguments.
    """
    args = []

    if self.build_source:
      args.append('--nousepkg')

    if self.toolchain_targets:
      args.extend(['--toolchain_boards', ','.join(self.toolchain_targets)])

    return args

def Create(arguments):
  """Create or replace the chroot.

  Args:
    arguments (CreateArguments): The various arguments to create a chroot.

  Returns:
    int - The version of the resulting chroot.
  """
  cros_build_lib.AssertOutsideChroot()

  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk')]
  cmd.extend(arguments.GetArgList())

  cros_build_lib.RunCommand(cmd)

  return GetChrootVersion()


def GetChrootVersion():
  """Get the chroot version.

  Returns:
    int|None - The version of the chroot if the chroot is valid, else None.
  """
  return cros_sdk_lib.GetChrootVersion(constants.DEFAULT_CHROOT_PATH)


def Update(arguments):
  """Update the chroot.

  Args:
    arguments (UpdateArguments): The various arguments for updating a chroot.

  Returns:
    int - The version of the chroot after the update.
  """
  # TODO: This should be able to be run either in or out of the chroot.
  cros_build_lib.AssertInsideChroot()

  cmd = [os.path.join(constants.CROSUTILS_DIR, 'update_chroot')]
  cmd.extend(arguments.GetArgList())

  cros_build_lib.RunCommand(cmd)

  return GetChrootVersion()

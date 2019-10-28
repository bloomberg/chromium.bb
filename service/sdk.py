# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Operations to work with the SDK chroot."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib


class ChrootPaths(object):
  """Value object to hold common cros_sdk path arguments."""

  def __init__(self, cache_dir=None, chroot_path=None):
    """Chroot paths init.

    Args:
      cache_dir (str): Override the default cache directory.
      chroot_path (str): Set the path the chroot resides (or will be created).
    """
    self.cache_dir = cache_dir
    self.chroot_path = chroot_path

  def GetArgList(self):
    """Get the list of the corresponding commandline arguments.

    Returns:
      list - The list of the corresponding command line arguments.
    """
    args = []

    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])

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

  @property
  def chroot_path(self):
    return self.paths.chroot_path


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
    else:
      args.append('--skip_toolchain_update')

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

  cros_build_lib.run(cmd)

  version = GetChrootVersion(arguments.chroot_path)
  if not arguments.replace:
    # Force replace scenarios. Only needed when we're not already replacing it.
    if not version:
      # Force replace when we can't get a version for a chroot that exists,
      # since something must have gone wrong.
      logging.notice('Replacing broken chroot.')
      arguments.replace = True
      return Create(arguments)
    elif not cros_sdk_lib.IsChrootVersionValid(arguments.chroot_path):
      # Force replace when the version is not valid, i.e. ahead of the chroot
      # version hooks.
      logging.notice('Replacing chroot ahead of current checkout.')
      arguments.replace = True
      return Create(arguments)
    elif not cros_sdk_lib.IsChrootDirValid(arguments.chroot_path):
      # Force replace when the permissions or owner are not correct.
      logging.notice('Replacing chroot with invalid permissions.')
      arguments.replace = True
      return Create(arguments)

  return GetChrootVersion(arguments.chroot_path)


def Delete(chroot_path=None):
  """Delete the chroot.

  Args:
    chroot_path: The chroot directory, or None to use the default.
  """
  # Get an order of magnitude on the size of the chroot we're trying to delete
  # to see if the size and disk performance are a major factor of the
  # SDK Delete timeout issue, or if it's another underlying problem.
  # TODO(crbug.com/1018217) Remove or make a proper metric.
  logging.info('Checking chroot size.')
  path = chroot_path or constants.DEFAULT_CHROOT_PATH
  cmd = ['du', '-h', '-d0', path]
  cros_build_lib.sudo_run(cmd)

  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'), '--delete']
  if chroot_path:
    cmd.extend(['--chroot', chroot_path])

  cros_build_lib.run(cmd)


def GetChrootVersion(chroot_path=None):
  """Get the chroot version.

  Args:
    chroot_path (str|None): The chroot path.

  Returns:
    int|None - The version of the chroot if the chroot is valid, else None.
  """
  if chroot_path:
    path = chroot_path
  elif cros_build_lib.IsInsideChroot():
    path = None
  else:
    path = constants.DEFAULT_CHROOT_PATH

  return cros_sdk_lib.GetChrootVersion(path)


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

  cros_build_lib.run(cmd)

  return GetChrootVersion()

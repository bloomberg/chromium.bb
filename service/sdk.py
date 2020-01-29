# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Operations to work with the SDK chroot."""

from __future__ import print_function

import os
import sys
import uuid

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class CreateArguments(object):
  """Value object to handle the chroot creation arguments."""

  def __init__(self, replace=False, bootstrap=False, use_image=True,
               chroot_path=None, cache_dir=None):
    """Create arguments init.

    Args:
      replace (bool): Whether an existing chroot should be deleted.
      bootstrap (bool): Whether to build the SDK from source.
      use_image (bool): Whether to mount the chroot on a loopback image or
        create it directly in a directory.
      chroot_path: Path to where the chroot should be reside.
      cache_dir: Alternative directory to use as a cache for the chroot.
    """
    self.replace = replace
    self.bootstrap = bootstrap
    self.use_image = use_image
    self.chroot_path = chroot_path
    self.cache_dir = cache_dir

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

    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])

    if self.chroot_path:
      args.extend(['--chroot', self.chroot_path])

    return args


class UpdateArguments(object):
  """Value object to handle the update arguments."""

  def __init__(self,
               build_source=False,
               toolchain_targets=None,
               toolchain_changed=False):
    """Update arguments init.

    Args:
      build_source (bool): Whether to build the source or use prebuilts.
      toolchain_targets (list): The list of build targets whose toolchains
        should be updated.
      toolchain_changed (bool): Whether a toolchain change has occurred. Implies
        build_source.
    """
    self.build_source = build_source or toolchain_changed
    self.toolchain_targets = toolchain_targets

  def GetArgList(self):
    """Get the list of the corresponding command line arguments.

    Returns:
      list - The list of the corresponding command line arguments.
    """
    args = []

    if self.build_source:
      args.append('--nousepkg')
    elif self.toolchain_targets:
      args.extend(['--toolchain_boards', ','.join(self.toolchain_targets)])

    return args


def Clean(chroot, images=False, sysroots=False, tmp=False):
  """Clean the chroot.

  See:
    cros clean -h

  Args:
    chroot: The chroot to clean.
    images (bool): Remove all built images.
    sysroots (bool): Remove all of the sysroots.
    tmp (bool): Clean the tmp/ directory.
  """
  if not images and not sysroots and not tmp:
    return

  cmd = ['cros', 'clean']
  if chroot:
    cmd.extend(['--sdk-path', chroot.path])
  if images:
    cmd.append('--images')
  if sysroots:
    cmd.append('--sysroots')
  if tmp:
    cmd.append('--chroot-tmp')

  cros_build_lib.run(cmd)


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


def Delete(chroot=None):
  """Delete the chroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot being deleted, or None for the
      default chroot.
  """
  # Manually remove the sysroots to reduce the time taken to delete the chroot.
  logging.info('Removing sysroots.')
  Clean(chroot, sysroots=True)

  # Delete the chroot itself.
  logging.info('Removing the SDK.')
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'), '--delete']
  if chroot:
    cmd.extend(['--chroot', chroot.path])

  cros_build_lib.run(cmd)

  # Remove any images that were built.
  logging.info('Removing images.')
  Clean(chroot, images=True)


def Unmount(chroot=None):
  """Unmount the chroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot being unmounted, or None for the
      default chroot.
  """
  logging.info('Unmounting the chroot.')
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'), '--unmount']
  if chroot:
    cmd.extend(['--chroot', chroot.path])

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

  # The sdk update uses splitdebug instead of separatedebug. Make sure
  # separatedebug is disabled and enable splitdebug.
  existing = os.environ.get('FEATURES', '')
  features = ' '.join((existing, '-separatedebug splitdebug')).strip()
  extra_env = {'FEATURES': features}

  cros_build_lib.run(cmd, extra_env=extra_env)

  return GetChrootVersion()


def CreateSnapshot(chroot=None, replace_if_needed=False):
  """Create a logical volume snapshot of a chroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot to perform the operation on.
    replace_if_needed (bool): If true, will replace the existing chroot with
      a new one capable of being mounted as a loopback image if needed.

  Returns:
    str - The name of the snapshot created.
  """
  _EnsureSnapshottableState(chroot, replace=replace_if_needed)

  snapshot_token = str(uuid.uuid4())
  logging.info('Creating SDK snapshot with token ID: %s', snapshot_token)

  cmd = [
      os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'),
      '--snapshot-create',
      snapshot_token,
  ]
  if chroot:
    cmd.extend(['--chroot', chroot.path])

  cros_build_lib.run(cmd)

  return snapshot_token


def RestoreSnapshot(snapshot_token, chroot=None):
  """Restore a logical volume snapshot of a chroot.

  Args:
    snapshot_token (str): The name of the snapshot to restore. Typically an
      opaque generated name returned from `CreateSnapshot`.
    chroot (chroot_lib.Chroot): The chroot to perform the operation on.
  """
  # Unmount to clean up stale processes that may still be in the chroot, in
  # order to prevent 'device busy' errors from umount.
  Unmount(chroot)
  logging.info('Restoring SDK snapshot with ID: %s', snapshot_token)
  cmd = [
      os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'),
      '--snapshot-restore',
      snapshot_token,
  ]
  if chroot:
    cmd.extend(['--chroot', chroot.path])

  # '--snapshot-restore' will automatically remount the image after restoring.
  cros_build_lib.run(cmd)


def _EnsureSnapshottableState(chroot=None, replace=False):
  """Ensures that a chroot is in a capable state to create an LVM snapshot.

  Args:
    chroot (chroot_lib.Chroot): The chroot to perform the operation on.
    replace (bool): If true, will replace the existing chroot with a new one
      capable of being mounted as a loopback image if needed.
  """
  cmd = [
      os.path.join(constants.CHROMITE_BIN_DIR, 'cros_sdk'),
      '--snapshot-list',
  ]
  if chroot:
    cmd.extend(['--chroot', chroot.path])

  cache_dir = chroot.cache_dir if chroot else None
  chroot_path = chroot.path if chroot else None

  res = cros_build_lib.run(cmd, check=False, encoding='utf-8')

  if res.returncode == 0:
    return
  elif 'Unable to find VG' in res.stderr and replace:
    logging.warn('SDK was created with nouse-image which does not support '
                 'snapshots. Recreating SDK to support snapshots.')

    args = CreateArguments(
        replace=True,
        bootstrap=False,
        use_image=True,
        cache_dir=cache_dir,
        chroot_path=chroot_path)

    Create(args)
    return
  else:
    res.check_returncode()

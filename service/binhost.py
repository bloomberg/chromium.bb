# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Binhost API interacts with Portage binhosts and Packages files."""

from __future__ import print_function

import functools
import os
import sys

from chromite.lib import binpkg
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util
from chromite.utils import key_value_store


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# The name of the ACL argument file.
_GOOGLESTORAGE_GSUTIL_FILE = 'googlestorage_acl.txt'

# The name of the package file (relative to sysroot) where the list of packages
# for dev-install is stored.
_DEV_INSTALL_PACKAGES_FILE = 'build/dev-install/package.installable'


class Error(Exception):
  """Base error class for the module."""


class EmptyPrebuiltsRoot(Error):
  """When a prebuilts root is unexpectedly empty."""


class NoAclFileFound(Error):
  """No ACL file could be found."""


def _ValidateBinhostConf(path, key):
  """Validates the binhost conf file defines only one environment variable.

  This function is effectively a sanity check that ensures unexpected
  configuration is not clobbered by conf overwrites.

  Args:
    path: Path to the file to validate.
    key: Expected binhost key.

  Raises:
    ValueError: If file defines != 1 environment variable.
  """
  if not os.path.exists(path):
    # If the conf file does not exist, e.g. with new targets, then whatever.
    return

  kvs = key_value_store.LoadFile(path)

  if not kvs:
    raise ValueError(
        'Found empty .conf file %s when a non-empty one was expected.' % path)
  elif len(kvs) > 1:
    raise ValueError(
        'Conf file %s must define exactly 1 variable. '
        'Instead found: %r' % (path, kvs))
  elif key not in kvs:
    raise KeyError('Did not find key %s in %s' % (key, path))


def _ValidatePrebuiltsFiles(prebuilts_root, prebuilts_paths):
  """Validate all prebuilt files exist.

  Args:
    prebuilts_root: Absolute path to root directory containing prebuilts.
    prebuilts_paths: List of file paths relative to root, to be verified.

  Raises:
    LookupError: If any prebuilt archive does not exist.
  """
  for prebuilt_path in prebuilts_paths:
    full_path = os.path.join(prebuilts_root, prebuilt_path)
    if not os.path.exists(full_path):
      raise LookupError('Prebuilt archive %s does not exist' % full_path)


def _ValidatePrebuiltsRoot(target, prebuilts_root):
  """Validate the given prebuilts root exists.

  If the root does not exist, it probably means the build target did not build
  successfully, so warn callers appropriately.

  Args:
    target: The build target in question.
    prebuilts_root: The expected root directory for the target's prebuilts.

  Raises:
    EmptyPrebuiltsRoot: If prebuilts root does not exist.
  """
  if not os.path.exists(prebuilts_root):
    raise EmptyPrebuiltsRoot(
        'Expected to find prebuilts for build target %s at %s. '
        'Did %s build successfully?' % (target, prebuilts_root, target))


def GetPrebuiltsRoot(chroot, sysroot, build_target):
  """Find the root directory with binary prebuilts for the given sysroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot where the sysroot lives.
    sysroot (sysroot_lib.Sysroot): The sysroot.
    build_target (build_target_lib.BuildTarget): The build target.

  Returns:
    Absolute path to the root directory with the target's prebuilt archives.
  """
  root = os.path.join(chroot.path, sysroot.path.lstrip(os.sep), 'packages')
  _ValidatePrebuiltsRoot(build_target, root)
  return root


def GetPrebuiltsFiles(prebuilts_root):
  """Find paths to prebuilts at the given root directory.

  Assumes the root contains a Portage package index named Packages.

  The package index paths are used to de-duplicate prebuilts uploaded. The
  immediate consequence of this is reduced storage usage. The non-obvious
  consequence is the shared packages generally end up with public permissions,
  while the board-specific packages end up with private permissions. This is
  what is meant to happen, but a further consequence of that is that when
  something happens that causes the toolchains to be uploaded as a private
  board's package, the board will not be able to build properly because
  it won't be able to fetch the toolchain packages, because they're expected
  to be public.

  Args:
    prebuilts_root: Absolute path to root directory containing a package index.

  Returns:
    List of paths to all prebuilt archives, relative to the root.
  """
  package_index = binpkg.GrabLocalPackageIndex(prebuilts_root)
  prebuilt_paths = []
  for package in package_index.packages:
    prebuilt_paths.append(package['CPV'] + '.tbz2')

    include_debug_symbols = package.get('DEBUG_SYMBOLS')
    if cros_build_lib.BooleanShellValue(include_debug_symbols, default=False):
      prebuilt_paths.append(package['CPV'] + '.debug.tbz2')

  _ValidatePrebuiltsFiles(prebuilts_root, prebuilt_paths)
  return prebuilt_paths


def UpdatePackageIndex(prebuilts_root, upload_uri, upload_path, sudo=False):
  """Update package index with information about where it will be uploaded.

  This causes the existing Packages file to be overwritten.

  Args:
    prebuilts_root: Absolute path to root directory containing binary prebuilts.
    upload_uri: The URI (typically GS bucket) where prebuilts will be uploaded.
    upload_path: The path at the URI for the prebuilts.
    sudo (bool): Whether to write the file as the root user.

  Returns:
    Path to the new Package index.
  """
  assert not upload_path.startswith('/')
  package_index = binpkg.GrabLocalPackageIndex(prebuilts_root)
  package_index.SetUploadLocation(upload_uri, upload_path)
  package_index.header['TTL'] = 60 * 60 * 24 * 365
  package_index_path = os.path.join(prebuilts_root, 'Packages')
  package_index.WriteFile(package_index_path, sudo=sudo)
  return package_index_path


def SetBinhost(target, key, uri, private=True):
  """Set binhost configuration for the given build target.

  A binhost is effectively a key (Portage env variable) pointing to a URL
  that contains binaries. The configuration is set in .conf files at static
  directories on a build target by build target (and host by host) basis.

  This function updates the .conf file by completely rewriting it.

  Args:
    target: The build target to set configuration for.
    key: The binhost key to set, e.g. POSTSUBMIT_BINHOST.
    uri: The new value for the binhost key, e.g. gs://chromeos-prebuilt/foo/bar.
    private: Whether or not the build target is private.

  Returns:
    Path to the updated .conf file.
  """
  conf_root = os.path.join(
      constants.SOURCE_ROOT,
      constants.PRIVATE_BINHOST_CONF_DIR if private else
      constants.PUBLIC_BINHOST_CONF_DIR, 'target')
  conf_file = '%s-%s.conf' % (target, key)
  conf_path = os.path.join(conf_root, conf_file)
  _ValidateBinhostConf(conf_path, key)
  osutils.WriteFile(conf_path, '%s="%s"' % (key, uri))
  return conf_path


def RegenBuildCache(chroot, overlay_type):
  """Regenerate the Build Cache for the given target.

  Args:
    chroot (chroot_lib): The chroot where the regen command will be run.
    overlay_type: one of "private", "public", or "both".

  Returns:
    list[str]: The overlays with updated caches.
  """
  overlays = portage_util.FindOverlays(overlay_type)

  task = functools.partial(
      portage_util.RegenCache, commit_changes=False, chroot=chroot)
  task_inputs = [[o] for o in overlays if os.path.isdir(o)]
  results = parallel.RunTasksInProcessPool(task, task_inputs)

  # Filter out all of the unchanged-overlay results.
  return [overlay_dir for overlay_dir in results if overlay_dir]


def GetPrebuiltAclArgs(build_target):
  """Read and parse the GS ACL file from the private overlays.

  Args:
    build_target (build_target_lib.BuildTarget): The build target.

  Returns:
    list[list[str]]: A list containing all of the [arg, value] pairs. E.g.
      [['-g', 'group_id:READ'], ['-u', 'user:FULL_CONTROL']]
  """
  acl_file = portage_util.FindOverlayFile(_GOOGLESTORAGE_GSUTIL_FILE,
                                          board=build_target.name)

  if not acl_file:
    raise NoAclFileFound('No ACL file found for %s.' % build_target.name)

  lines = osutils.ReadFile(acl_file).splitlines()
  # Remove comments.
  lines = [line.split('#', 1)[0].strip() for line in lines]
  # Remove empty lines.
  lines = [line.strip() for line in lines if line.strip()]

  return [line.split() for line in lines]


def GetBinhosts(build_target):
  """Get the binhosts for the build target.

  Args:
    build_target (build_target_lib.BuildTarget): The build target.

  Returns:
    list[str]: The build target's binhosts.
  """
  binhosts = portage_util.PortageqEnvvar('PORTAGE_BINHOST',
                                         board=build_target.name,
                                         allow_undefined=True)
  return binhosts.split() if binhosts else []


def ReadDevInstallPackageFile(filename):
  """Parse the dev-install package file.

  Args:
    filename (str): The full path to the dev-install package list.

  Returns:
    list[str]: The packages in the package list file.
  """
  with open(filename) as f:
    return [line.strip() for line in f]


def ReadDevInstallFilesToCreatePackageIndex(chroot, sysroot, package_index_path,
                                            upload_uri, upload_path):
  """Create dev-install Package index specified by package_index_path

  The current Packages file is read and a new Packages file is created based
  on the subset of packages in the _DEV_INSTALL_PACKAGES_FILE.

  Args:
    chroot (chroot_lib.Chroot): The chroot where the sysroot lives.
    sysroot (sysroot_lib.Sysroot): The sysroot.
    package_index_path (str): Path to the Packages file to be created.
    upload_uri: The URI (typically GS bucket) where prebuilts will be uploaded.
    upload_path: The path at the URI for the prebuilts.

  Returns:
    list[str]: The list of packages contained in package_index_path,
      where each package string is a category/file.
  """
  # Read the dev-install binhost package file
  devinstall_binhost_filename = chroot.full_path(sysroot.path,
                                                 _DEV_INSTALL_PACKAGES_FILE)
  devinstall_package_list = ReadDevInstallPackageFile(
      devinstall_binhost_filename)

  # Read the Packages file, remove packages not in package_list
  package_path = chroot.full_path(sysroot.path, 'packages')
  CreateFilteredPackageIndex(package_path, devinstall_package_list,
                             package_index_path,
                             upload_uri, upload_path)

  # We have the list of packages, create full path and verify each one.
  upload_targets_list = GetPrebuiltsForPackages(
      package_path, devinstall_package_list)

  return upload_targets_list


def CreateFilteredPackageIndex(package_path, devinstall_package_list,
                               package_index_path,
                               upload_uri, upload_path, sudo=False):
  """Create Package file for dev-install process.

  The created package file (package_index_path) contains only the
  packages from the system packages file (in package_path) that are in the
  devinstall_package_list. The new package file will use the provided values
  for upload_uri and upload_path.

  Args:
    package_path (str): Absolute path to the standard Packages file.
    devinstall_package_list (list[str]): Packages from packages.installable
    package_index_path (str): Absolute path for new Packages file.
    upload_uri (str): The URI where prebuilts will be uploaded.
    upload_path (str): The path at the URI for the prebuilts.
    sudo (bool): Whether to write the file as the root user.
  """


  def ShouldFilterPackage(package):
    """Local func to filter packages not in the devinstall_package_list

    Args:
      package (dict): Dictionary with key 'CPV' and package name as value

    Returns:
      True (filter) if not in the devinstall_package_list, else False (don't
        filter) if in the devinstall_package_list
    """
    value = package['CPV']
    if value in devinstall_package_list:
      return False
    else:
      return True

  package_index = binpkg.GrabLocalPackageIndex(package_path)
  package_index.RemoveFilteredPackages(ShouldFilterPackage)
  package_index.SetUploadLocation(upload_uri, upload_path)
  package_index.header['TTL'] = 60 * 60 * 24 * 365
  package_index.WriteFile(package_index_path, sudo=sudo)


def GetPrebuiltsForPackages(package_root, package_list):
  """Create list of file paths for the package list and validate they exist.

  Args:
    package_root (str): Path to 'packages' directory.
    package_list (list[str]): List of packages.

  Returns:
    List of validated targets.
  """
  upload_targets_list = []
  for pkg in package_list:
    zip_target = pkg + '.tbz2'
    upload_targets_list.append(zip_target)
    full_pkg_path = os.path.join(package_root, pkg) + '.tbz2'
    if not os.path.exists(full_pkg_path):
      raise LookupError('DevInstall archive %s does not exist' % full_pkg_path)
  return upload_targets_list

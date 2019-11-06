# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Binhost API interacts with Portage binhosts and Packages files."""

from __future__ import print_function

import os

from chromite.lib import binpkg
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util


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

  kvs = cros_build_lib.LoadKeyValueFile(path)

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
    LookupError: If prebuilts root does not exist.
  """
  if not os.path.exists(prebuilts_root):
    raise LookupError(
        'Expected to find prebuilts for build target %s at %s. '
        'Did %s build successfully?' % (target, prebuilts_root, target))


def GetPrebuiltsRoot(target):
  """Find the root directory with binary prebuilts for the given build target.

  Args:
    target: The build target in question.

  Returns:
    Absolute path to the root directory with the target's prebuilt archives.
  """
  root = os.path.join(constants.SOURCE_ROOT, 'chroot/build', target, 'packages')
  _ValidatePrebuiltsRoot(target, root)
  return root


def GetPrebuiltsFiles(prebuilts_root):
  """Find paths to prebuilts at the given root directory.

  Assumes the root contains a Portage package index named Packages.

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

def RegenBuildCache(overlay_type, sysroot_path):
  """Regenerate the Build Cache for the given target.

  Args:
    overlay_type: one of "private", "public", or "both".
    sysroot_path: Sysroot to update.
  """
  overlays = portage_util.FindOverlays(overlay_type, buildroot=sysroot_path)
  task_inputs = [[o] for o in overlays if os.path.isdir(o)]
  parallel.RunTasksInProcessPool(portage_util.RegenCache, task_inputs)

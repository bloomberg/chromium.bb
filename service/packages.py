# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package utility functionality."""

from __future__ import print_function

import functools
import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import uprev_lib

# Registered handlers for uprevving versioned packages.
_UPREV_FUNCS = {}


class Error(Exception):
  """Module's base error class."""


class UprevError(Error):
  """An error occurred while uprevving packages."""


class UnknownPackageError(Error):
  """Uprev attempted for a package without a registered handler."""


def uprevs_versioned_package(package):
  """Decorator to register package uprev handlers."""
  assert package

  def register(func):
    """Registers |func| as a handler for |package|."""
    _UPREV_FUNCS[package] = func

    @functools.wraps(func)
    def pass_through(*args, **kwargs):
      return func(*args, **kwargs)

    return pass_through

  return register


def uprev_build_targets(build_targets, overlay_type, chroot=None,
                        output_dir=None):
  """Uprev the set provided build targets, or all if not specified.

  Args:
    build_targets (list[build_target_util.BuildTarget]|None): The build targets
      whose overlays should be uprevved, empty or None for all.
    overlay_type (str): One of the valid overlay types except None (see
      constants.VALID_OVERLAYS).
    chroot (chroot_lib.Chroot|None): The chroot to clean, if desired.
    output_dir (str|None): The path to optionally dump result files.
  """
  # Need a valid overlay, but exclude None.
  assert overlay_type and overlay_type in constants.VALID_OVERLAYS

  if build_targets:
    overlays = portage_util.FindOverlaysForBoards(
        overlay_type, boards=[t.name for t in build_targets])
  else:
    overlays = portage_util.FindOverlays(overlay_type)

  return uprev_overlays(overlays, build_targets=build_targets, chroot=chroot,
                        output_dir=output_dir)


def uprev_overlays(overlays, build_targets=None, chroot=None, output_dir=None):
  """Uprev the given overlays.

  Args:
    overlays (list[str]): The list of overlay paths.
    build_targets (list[build_target_util.BuildTarget]|None): The build targets
      to clean in |chroot|, if desired. No effect unless |chroot| is provided.
    chroot (chroot_lib.Chroot|None): The chroot to clean, if desired.
    output_dir (str|None): The path to optionally dump result files.

  Returns:
    list[str] - The paths to all of the modified ebuild files. This includes the
      new files that were added (i.e. the new versions) and all of the removed
      files (i.e. the old versions).
  """
  assert overlays

  manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)

  uprev_manager = uprev_lib.UprevOverlayManager(overlays, manifest,
                                                build_targets=build_targets,
                                                chroot=chroot,
                                                output_dir=output_dir)
  uprev_manager.uprev()

  return uprev_manager.modified_ebuilds


def uprev_versioned_package(package, build_targets, refs, chroot):
  """Call registered uprev handler function for the package.

  Args:
    package (portage_util.CPV): The package being uprevved.
    build_targets (list[build_target_util.BuildTarget]): The build targets to
        clean on a successful uprev.
    refs (list[uprev_lib.GitRef]):
    chroot (chroot_lib.Chroot): The chroot to enter for cleaning.

  Returns:
    list[str]: The list of modified ebuilds.
  """
  assert package

  if package.cp not in _UPREV_FUNCS:
    raise UnknownPackageError(
        'Package "%s" does not have a registered handler.' % package.cp)

  return _UPREV_FUNCS[package.cp](build_targets, refs, chroot)


# TODO(evanhernandez): Remove this. Only a quick hack for testing.
@uprevs_versioned_package('sample/sample')
def uprev_sample(*_args, **_kwargs):
  """Mimics an uprev by changing files in sandbox repos.

  See: uprev_versioned_package.
  """
  paths = [
      os.path.join(constants.SOURCE_ROOT, 'infra/dummies', repo, 'sample.txt')
      for repo in ('general-sandbox', 'merge-sandbox')
  ]

  for path in paths:
    osutils.WriteFile(path, cros_build_lib.GetRandomString())

  return paths


@uprevs_versioned_package(constants.CHROME_CP)
def uprev_chrome(build_targets, refs, chroot):
  """Uprev chrome and its related packages.

  See: uprev_versioned_package.
  """
  # Determine the version from the refs (tags), i.e. the chrome versions are the
  # tag names.
  chrome_version = uprev_lib.get_chrome_version_from_refs(refs)

  uprev_manager = uprev_lib.UprevChromeManager(
      chrome_version, build_targets=build_targets, chroot=chroot)
  # Start with chrome itself, as we can't do anything else unless chrome
  # uprevs successfully.
  if not uprev_manager.uprev(constants.CHROME_CP):
    return []

  # With a successful chrome rev, also uprev related packages.
  for package in constants.OTHER_CHROME_PACKAGES:
    uprev_manager.uprev(package)

  return uprev_manager.modified_ebuilds


def get_best_visible(atom):
  """Returns the best visible CPV for the given atom."""
  assert atom
  return portage_util.PortageqBestVisible(atom)

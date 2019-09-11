# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package utility functionality."""

from __future__ import print_function

import functools
import json
import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import portage_util
from chromite.lib import uprev_lib

# Registered handlers for uprevving versioned packages.
_UPREV_FUNCS = {}


class Error(Exception):
  """Module's base error class."""


class UnknownPackageError(Error):
  """Uprev attempted for a package without a registered handler."""


class UprevError(Error):
  """An error occurred while uprevving packages."""


class AndroidIsPinnedUprevError(UprevError):
  """Raised when we try to uprev while Android is pinned."""

  def __init__(self, new_android_atom):
    """Initialize a AndroidIsPinnedUprevError.

    Args:
      new_android_atom: The Android atom that we failed to
                        uprev to, due to Android being pinned.
    """
    assert new_android_atom
    msg = ('Failed up uprev to Android version %s as Android was pinned.' %
           new_android_atom)
    super(AndroidIsPinnedUprevError, self).__init__(msg)
    self.new_android_atom = new_android_atom


class EbuildManifestError(Error):
  """Error when running ebuild manifest."""


class UprevVersionedPackageResult(object):
  """Result data object for uprev_versioned_package."""

  def __init__(self, new_version=None, modified_ebuilds=None):
    self.new_version = new_version
    self.modified_ebuilds = modified_ebuilds or []

  @property
  def uprevved(self):
    return bool(self.new_version)


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


def uprev_android(tracking_branch, android_package, android_build_branch,
                  chroot, build_targets=None, android_version=None,
                  android_gts_build_branch=None):
  """Returns the portage atom for the revved Android ebuild - see man emerge."""
  command = ['cros_mark_android_as_stable',
             '--tracking_branch=%s' % tracking_branch,
             '--android_package=%s' % android_package,
             '--android_build_branch=%s' % android_build_branch]
  if build_targets:
    command.append('--boards=%s' % ':'.join(bt.name for bt in build_targets))
  if android_version:
    command.append('--force_version=%s' % android_version)
  if android_gts_build_branch:
    command.append('--android_gts_build_branch=%s' % android_gts_build_branch)

  result = cros_build_lib.RunCommand(command, redirect_stdout=True,
                                     enter_chroot=True,
                                     chroot_args=chroot.get_enter_args())

  android_atom = _parse_android_atom(result)
  if not android_atom:
    logging.info('Found nothing to rev.')
    return None

  for target in build_targets or []:
    # Sanity check: We should always be able to merge the version of
    # Android we just unmasked.
    command = ['emerge-%s' % target.name, '-p', '--quiet',
               '=%s' % android_atom]
    try:
      cros_build_lib.RunCommand(command, enter_chroot=True,
                                chroot_args=chroot.get_enter_args())
    except cros_build_lib.RunCommandError:
      logging.error(
          'Cannot emerge-%s =%s\nIs Android pinned to an older '
          'version?', target, android_atom)
      raise AndroidIsPinnedUprevError(android_atom)

  return android_atom


def _parse_android_atom(result):
  """Helper to parse the atom from the cros_mark_android_as_stable output.

  This function is largely just intended to make testing easier.

  Args:
    result (cros_build_lib.CommandResult): The cros_mark_android_as_stable
      command result.
  """
  portage_atom_string = result.output.strip()

  android_atom = None
  if portage_atom_string:
    android_atom = portage_atom_string.splitlines()[-1].partition('=')[-1]

  return android_atom


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
    UprevVersionedPackageResult: The result.
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

  return UprevVersionedPackageResult('1.2.3', paths)


@uprevs_versioned_package('afdo/kernel-profiles')
def uprev_kernel_afdo(*_args, **_kwargs):
  """Updates kernel ebuilds with versions from kernel_afdo.json.

  See: uprev_versioned_package.

  Raises:
    EbuildManifestError: When ebuild manifest does not complete successfuly.
  """
  path = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                      'toolchain-utils', 'afdo_metadata', 'kernel_afdo.json')

  with open(path, 'r') as f:
    versions = json.load(f)

  paths = []
  for version, version_info in versions.items():
    path = os.path.join('src', 'third_party', 'chromiumos-overlay',
                        'sys-kernel', version)
    ebuild_path = os.path.join(constants.SOURCE_ROOT, path,
                               '%s-9999.ebuild' % version)
    portage_util.EBuild.UpdateEBuild(
        ebuild_path,
        dict(AFDO_PROFILE_VERSION=version_info['name']),
        make_stable=False)
    paths.append(ebuild_path)

    cmd = [
        'ebuild',
        os.path.join(constants.CHROOT_SOURCE_ROOT, ebuild_path), 'manifest',
        '--force'
    ]

    try:
      cros_build_lib.RunCommand(cmd, enter_chroot=True)
    except cros_build_lib.RunCommandError as e:
      raise EbuildManifestError(
          'Error encountered when regenerating the manifest for ebuild: %s\n%s'
          % (ebuild_path, e), e)

    manifest_path = os.path.join(constants.SOURCE_ROOT, path, 'Manifest')
    paths.append(manifest_path)

  return UprevVersionedPackageResult('test version', paths)


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

  return UprevVersionedPackageResult(chrome_version,
                                     uprev_manager.modified_ebuilds)


def get_best_visible(atom, build_target=None):
  """Returns the best visible CPV for the given atom.

  Args:
    atom (str): The atom to look up.
    build_target (build_target_util.BuildTarget): The build target whose
        sysroot should be searched.
  """
  assert atom

  board = build_target.name if build_target else None
  return portage_util.PortageqBestVisible(atom, board=board)

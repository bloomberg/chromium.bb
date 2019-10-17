# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package utility functionality."""

from __future__ import print_function

import collections
import fileinput
import functools
import json
import os
import re
import sys


from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import uprev_lib

if cros_build_lib.IsInsideChroot():
  from chromite.service import dependency

# Registered handlers for uprevving versioned packages.
_UPREV_FUNCS = {}


class Error(Exception):
  """Module's base error class."""


class UnknownPackageError(Error):
  """Uprev attempted for a package without a registered handler."""


class UprevError(Error):
  """An error occurred while uprevving packages."""


class NoAndroidVersionError(Error):
  """An error occurred while trying to determine the android version."""


class NoAndroidBranchError(Error):
  """An error occurred while trying to determine the android branch."""


class NoAndroidTargetError(Error):
  """An error occurred while trying to determine the android target."""


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


UprevVersionedPackageModifications = collections.namedtuple(
    'UprevVersionedPackageModifications', ('new_version', 'files'))


class UprevVersionedPackageResult(object):
  """Data object for uprev_versioned_package."""

  def __init__(self):
    self.modified = []

  def add_result(self, new_version, modified_files):
    """Adds version/ebuilds tuple to result.

    Args:
      new_version: New version number of package.
      modified_files: List of files modified for the given version.
    """
    result = UprevVersionedPackageModifications(new_version, modified_files)
    self.modified.append(result)
    return self

  @property
  def uprevved(self):
    return bool(self.modified)


def patch_ebuild_vars(ebuild_path, variables):
  """Updates variables in ebuild.

  Use this function rather than portage_util.EBuild.UpdateEBuild when you
  want to preserve the variable position and quotes within the ebuild.

  Args:
    ebuild_path: The path of the ebuild.
    variables: Dictionary of variables to update in ebuild.
  """
  try:
    for line in fileinput.input(ebuild_path, inplace=1):
      varname, eq, _ = line.partition('=')
      if eq == '=' and varname.strip() in variables:
        value = variables[varname]
        sys.stdout.write('%s="%s"\n' % (varname, value))
      else:
        sys.stdout.write(line)
  finally:
    fileinput.close()


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

  result = cros_build_lib.run(command, redirect_stdout=True,
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
      cros_build_lib.run(command, enter_chroot=True,
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

  return UprevVersionedPackageResult().add_result('1.2.3', paths)


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

  result = UprevVersionedPackageResult()
  for version, version_info in versions.items():
    path = os.path.join('src', 'third_party', 'chromiumos-overlay',
                        'sys-kernel', version)
    ebuild_path = os.path.join(constants.SOURCE_ROOT, path,
                               '%s-9999.ebuild' % version)
    chroot_ebuild_path = os.path.join(constants.CHROOT_SOURCE_ROOT, path,
                                      '%s-9999.ebuild' % version)
    afdo_profile_version = version_info['name']
    patch_ebuild_vars(ebuild_path,
                      dict(AFDO_PROFILE_VERSION=afdo_profile_version))

    try:
      cmd = ['ebuild', chroot_ebuild_path, 'manifest', '--force']
      cros_build_lib.run(cmd, enter_chroot=True)
    except cros_build_lib.RunCommandError as e:
      raise EbuildManifestError(
          'Error encountered when regenerating the manifest for ebuild: %s\n%s'
          % (chroot_ebuild_path, e), e)

    manifest_path = os.path.join(constants.SOURCE_ROOT, path, 'Manifest')

    result.add_result(afdo_profile_version, [ebuild_path, manifest_path])

  return result


@uprevs_versioned_package('afdo/chrome-profiles')
def uprev_chrome_afdo(*_args, **_kwargs):
  """Updates chrome ebuilds with versions from chrome_afdo.json.

  See: uprev_versioned_package.

  Raises:
    EbuildManifestError: When ebuild manifest does not complete successfuly.
  """
  path = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                      'toolchain-utils', 'afdo_metadata', 'chrome_afdo.json')

  with open(path, 'r') as f:
    versions = json.load(f)

  path = os.path.join('src', 'third_party', 'chromiumos-overlay',
                      'chromeos-base', 'chromeos-chrome')
  ebuild_path = os.path.join(constants.SOURCE_ROOT, path,
                             'chromeos-chrome-9999.ebuild')
  chroot_ebuild_path = os.path.join(constants.CHROOT_SOURCE_ROOT, path,
                                    'chromeos-chrome-9999.ebuild')

  result = UprevVersionedPackageResult()
  for version, version_info in versions.items():
    afdo_profile_version = version_info['name']
    varname = 'AFDO_FILE["%s"]' % version
    patch_ebuild_vars(ebuild_path, {varname: afdo_profile_version})

  try:
    cmd = ['ebuild', chroot_ebuild_path, 'manifest', '--force']
    cros_build_lib.run(cmd, enter_chroot=True)
  except cros_build_lib.RunCommandError as e:
    raise EbuildManifestError(
        'Error encountered when regenerating the manifest for ebuild: %s\n%s' %
        (chroot_ebuild_path, e), e)

  manifest_path = os.path.join(constants.SOURCE_ROOT, path, 'Manifest')
  result.add_result('chromeos-chrome', [ebuild_path, manifest_path])

  return result


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
  result = UprevVersionedPackageResult()
  # Start with chrome itself, as we can't do anything else unless chrome
  # uprevs successfully.
  if not uprev_manager.uprev(constants.CHROME_CP):
    return result

  # With a successful chrome rev, also uprev related packages.
  for package in constants.OTHER_CHROME_PACKAGES:
    uprev_manager.uprev(package)

  return result.add_result(chrome_version, uprev_manager.modified_ebuilds)


def get_best_visible(atom, build_target=None):
  """Returns the best visible CPV for the given atom.

  Args:
    atom (str): The atom to look up.
    build_target (build_target_util.BuildTarget): The build target whose
        sysroot should be searched, or the SDK if not provided.
  """
  assert atom

  board = build_target.name if build_target else None
  return portage_util.PortageqBestVisible(atom, board=board)


def has_prebuilt(atom, build_target=None):
  """Check if a prebuilt exists.

  Args:
    atom (str): The package whose prebuilt is being queried.
    build_target (build_target_util.BuildTarget): The build target whose
        sysroot should be searched, or the SDK if not provided.
  """
  assert atom

  board = build_target.name if build_target else None
  return portage_util.HasPrebuilt(atom, board=board)


def builds(atom, build_target, packages=None):
  """Check if |build_target| builds |atom| (has it in its depgraph)."""
  cros_build_lib.AssertInsideChroot()

  graph = dependency.GetBuildDependency(build_target.name, packages)
  return any(atom in package for package in graph['package_deps'])


def determine_chrome_version(build_target):
  """Returns the current Chrome version for the board (or in buildroot).

  Args:
    build_target (build_target_util.BuildTarget): The board build target.
  """
  cpv = portage_util.PortageqBestVisible(constants.CHROME_CP, build_target.name,
                                         cwd=constants.SOURCE_ROOT)
  # Something like 78.0.3877.4_rc -> 78.0.3877.4
  return cpv.version_no_rev.partition('_')[0]


def determine_android_package(board):
  """Returns the active Android container package in use by the board.

  Args:
    board: The board name this is specific to.
  """
  packages = portage_util.GetPackageDependencies(board, 'virtual/target-os')
  # We assume there is only one Android package in the depgraph.
  for package in packages:
    if package.startswith('chromeos-base/android-container-') or \
       package.startswith('chromeos-base/android-vm-'):
      return package
  return None


def determine_android_version(boards=None):
  """Determine the current Android version in buildroot now and return it.

  This uses the typical portage logic to determine which version of Android
  is active right now in the buildroot.

  Args:
    boards: List of boards to check version of.

  Returns:
    The Android build ID of the container for the boards.

  Raises:
    NoAndroidVersionError: if no unique Android version can be determined.
  """
  if not boards:
    return None
  # Verify that all boards have the same version.
  version = None
  for board in boards:
    package = determine_android_package(board)
    if not package:
      return None
    cpv = portage_util.SplitCPV(package)
    if not cpv:
      raise NoAndroidVersionError(
          'Android version could not be determined for %s' % board)
    if not version:
      version = cpv.version_no_rev
    elif version != cpv.version_no_rev:
      raise NoAndroidVersionError(
          'Different Android versions (%s vs %s) for %s' %
          (version, cpv.version_no_rev, boards))
  return version

def determine_android_branch(board):
  """Returns the Android branch in use by the active container ebuild."""
  try:
    android_package = determine_android_package(board)
  except cros_build_lib.RunCommandError:
    raise NoAndroidBranchError(
        'Android branch could not be determined for %s' % board)
  if not android_package:
    return None
  ebuild_path = portage_util.FindEbuildForBoardPackage(android_package, board)
  # We assume all targets pull from the same branch and that we always
  # have an ARM_TARGET, ARM_USERDEBUG_TARGET, or an X86_USERDEBUG_TARGET.
  targets = ['ARM_TARGET', 'ARM_USERDEBUG_TARGET', 'X86_USERDEBUG_TARGET']
  ebuild_content = osutils.SourceEnvironment(ebuild_path, targets)
  for target in targets:
    if target in ebuild_content:
      branch = re.search(r'(.*?)-linux-', ebuild_content[target])
      if branch is not None:
        return branch.group(1)
  raise NoAndroidBranchError(
      'Android branch could not be determined for %s (ebuild empty?)' % board)


def determine_android_target(board):
  try:
    android_package = determine_android_package(board)
  except cros_build_lib.RunCommandError:
    raise NoAndroidTargetError(
        'Android Target could not be determined for %s' % board)
  if not android_package:
    return None
  if android_package.startswith('chromeos-base/android-vm-'):
    return 'bertha'
  elif android_package.startswith('chromeos-base/android-container-'):
    return 'cheets'

  raise NoAndroidTargetError(
      'Android Target cannot be determined for the package: %s' %
      android_package)

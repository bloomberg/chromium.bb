# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Deps analysis service."""

from __future__ import print_function

import fileinput
import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.scripts import cros_extract_deps


def NormalizeSourcePaths(source_paths):
  """Return the "normalized" form of a list of source paths.

  Normalizing includes:
    * Sorting the source paths in alphabetical order.
    * Remove paths that are sub-path of others in the source paths.
    * Ensure all the directory path strings are ended with the trailing '/'.
    * Convert all the path from absolute paths to relative path (relative to
      the chroot source root).
  """
  for i, path in enumerate(source_paths):
    assert os.path.isabs(path), 'path %s is not an aboslute path' % path
    source_paths[i] = os.path.normpath(path)

  source_paths.sort()

  results = []

  for i, path in enumerate(source_paths):
    is_subpath_of_other = False
    for j, other in enumerate(source_paths):
      if j != i and osutils.IsSubPath(path, other):
        is_subpath_of_other = True
    if not is_subpath_of_other:
      if os.path.isdir(path) and not path.endswith('/'):
        path += '/'
      path = os.path.relpath(path, constants.CHROOT_SOURCE_ROOT)
      results.append(path)

  return results


def GenerateSourcePathMapping(packages, board):
  """Returns a map from each package to the source paths it depends on.

  A source path is considered dependency of a package if modifying files in that
  path might change the content of the resulting package.

  Notes:
    1) This method errs on the side of returning unneeded dependent paths.
       i.e: for a given package X, some of its dependency source paths may
       contain files which doesn't affect the content of X.

       On the other hands, any missing dependency source paths for package X is
       considered a bug.
    2) This only outputs the direct dependency source paths for a given package
       and does not takes include the dependency source paths of dependency
       packages.
       e.g: if package A depends on B (DEPEND=B), then results of computing
       dependency source paths of A doesn't include dependency source paths
       of B.

  Args:
    packages: The list of packages CPV names (str)
    board (str): The name of the board if packages are dependency of board. If
      the packages are board agnostic, then this should be None.

  Returns:
    Map from each package to the source path (relative to the repo checkout
      root, i.e: ~/trunk/ in your cros_sdk) it depends on.
    For each source path which is a directory, the string is ended with a
      trailing '/'.
  """

  results = {}

  packages_to_ebuild_paths = portage_util.FindEbuildsForPackages(
      packages, sysroot=cros_build_lib.GetSysroot(board), error_code_ok=False)

  # Source paths which are the directory of ebuild files.
  for package, ebuild_path in packages_to_ebuild_paths.items():
    # Include the entire directory that contains the ebuild as the package's
    # FILESDIR probably lives there too.
    results[package] = [os.path.dirname(ebuild_path)]

  # Source paths which are cros workon source paths.
  buildroot = os.path.join(constants.CHROOT_SOURCE_ROOT, 'src')
  manifest = git.ManifestCheckout.Cached(buildroot)
  for package, ebuild_path in packages_to_ebuild_paths.items():
    is_workon, _, is_blacklisted, _ = portage_util.EBuild.Classify(ebuild_path)
    if (not is_workon or
        # Blacklisted ebuild is pinned to a specific git sha1, so change in
        # that repo matter to the ebuild.
        is_blacklisted):
      continue
    ebuild = portage_util.EBuild(ebuild_path)
    workon_subtrees = ebuild.GetSourceInfo(buildroot, manifest).subtrees
    for path in workon_subtrees:
      results[package].append(path)

  # Source paths which are the eclasses which ebuilds inherit from.
  # For now, we just include all the whole eclass directory.
  # TODO(crbug.com/917174): for each package, expand the enalysis to output
  # only the path to eclass files which the packakge depends on.
  # Eclasses can live in either chromiumos-overlay, portage-stable, or
  # eclass-overlay. Without inspecting which eclasses are actually inherited
  # by a given ebuild we must pessimistically include all three.
  _ECLASS_DIRS = [
      os.path.join(constants.CHROOT_SOURCE_ROOT,
                   constants.CHROMIUMOS_OVERLAY_DIR, 'eclass'),
      os.path.join(constants.CHROOT_SOURCE_ROOT,
                   constants.PORTAGE_STABLE_OVERLAY_DIR, 'eclass'),
      os.path.join(constants.CHROOT_SOURCE_ROOT, constants.ECLASS_OVERLAY_DIR,
                   'eclass'),
  ]
  for package, ebuild_path in packages_to_ebuild_paths.items():
    use_inherit = False
    for line in fileinput.input(ebuild_path):
      if line.startswith('inherit '):
        use_inherit = True
    if use_inherit:
      results[package].extend(_ECLASS_DIRS)

  # Source paths which are the overlay directories for the given board
  # (packages are board specific).
  if board:
    overlay_directories = portage_util.FindOverlays(
        overlay_type='both', board=board)

    # The only parts of the overlay that affect every package are the current
    # profile (which lives somewhere in the profiles/ subdir) and a top-level
    # make.conf (if it exists).
    profile_directories = [
        os.path.join(x, 'profiles') for x in overlay_directories
    ]
    make_conf_paths = [
        os.path.join(x, 'make.conf') for x in overlay_directories
    ]

    # These directories *might* affect a build, so we include them for now to
    # be safe.
    metadata_directories = [
        os.path.join(x, 'metadata') for x in overlay_directories
    ]
    scripts_directories = [
        os.path.join(x, 'scripts') for x in overlay_directories
    ]

    for package in results:
      results[package].extend(profile_directories)
      results[package].extend(make_conf_paths)
      results[package].extend(metadata_directories)
      results[package].extend(scripts_directories)

  # chromiumos-overlay specifies default settings for every target in
  # chromeos/config  and so can potentially affect every board.
  for package in results:
    results[package].append(
        os.path.join(constants.CHROOT_SOURCE_ROOT,
                     constants.CHROMIUMOS_OVERLAY_DIR, 'chromeos', 'config'))

  for p in results:
    results[p] = NormalizeSourcePaths(results[p])

  return results


def GetBuildDependency(board, packages=None):
  """Return the build dependency and package -> source path map for |board|.

  Args:
    board (str): The name of the board whose artifacts are being created.
    packages (list[CPV]): The packages that need to be built, or empty / None
        to use the default list.

  Returns:
    JSON build dependencies report for the given board which includes:
      - Package level deps graph from portage
      - Map from each package to the source path
      (relative to the repo checkout root, i.e: ~/trunk/ in your cros_sdk) it
      depends on
  """
  results = {}
  results['target_board'] = board
  results['package_deps'] = {}
  results['source_path_mapping'] = {}

  board_specific_packages = []
  if packages:
    board_specific_packages.extend([cpv.cp for cpv in packages])
  else:
    board_specific_packages.extend([
        'virtual/target-os', 'virtual/target-os-dev', 'virtual/target-os-test',
        'virtual/target-os-factory'
    ])
    # Since we don’t have a clear mapping from autotests to git repos
    # and/or portage packages, we assume every board run all autotests.
    board_specific_packages += ['chromeos-base/autotest-all']

  non_board_specific_packages = [
      'virtual/target-sdk', 'chromeos-base/chromite',
      'virtual/target-sdk-post-cross'
  ]

  board_specific_deps = cros_extract_deps.ExtractDeps(
      sysroot=cros_build_lib.GetSysroot(board),
      package_list=board_specific_packages)

  non_board_specific_deps = cros_extract_deps.ExtractDeps(
      sysroot=cros_build_lib.GetSysroot(None),
      package_list=non_board_specific_packages)

  results['package_deps'].update(board_specific_deps)
  results['package_deps'].update(non_board_specific_deps)

  results['source_path_mapping'].update(
      GenerateSourcePathMapping(list(board_specific_deps), board))

  results['source_path_mapping'].update(
      GenerateSourcePathMapping(list(non_board_specific_deps), None))

  return results

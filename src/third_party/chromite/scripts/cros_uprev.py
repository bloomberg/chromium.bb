# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simplified cros_mark_as_stable script."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util

from chromite.cbuildbot import manifest_version

# Commit message subject for uprevving Portage packages.
GIT_COMMIT_SUBJECT = 'Marking set of ebuilds as stable'

# Commit message for uprevving Portage packages.
_GIT_COMMIT_MESSAGE = 'Marking 9999 ebuild for %s as stable.'


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--all', action='store_true',
                      help='Mark all packages as stable.')
  parser.add_argument('--boards', help='Colon-separated list of boards.')
  parser.add_argument('--chroot', type='path', help='The chroot path.')
  parser.add_argument('--force', action='store_true',
                      help='Force the stabilization of blacklisted packages. '
                           '(only compatible with -p)')
  parser.add_argument('--overlay-type', required=True,
                      choices=['public', 'private', 'both'],
                      help='Populates --overlays based on "public", "private", '
                           'or "both".')
  parser.add_argument('--packages',
                      help='Colon separated list of packages to rev.')
  parser.add_argument('--verbose', action='store_true',
                      help='Prints out debug info.')
  parser.add_argument('--dump-files', action='store_true',
                      help="Dump the revved packages, new files list, and "
                           "removed files list files. This is mostly for"
                           "debugging right now.")

  return parser


def _ParseArguments(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  options = parser.parse_args(argv)

  # Parse, cleanup, and populate options.
  if options.packages:
    options.packages = options.packages.split(':')
  if options.boards:
    options.boards = options.boards.split(':')
  options.overlays = portage_util.FindOverlays(options.overlay_type)

  # Verify options.
  if not options.packages and not options.all:
    parser.error('Please specify at least one package (--packages)')
  if options.force and options.all:
    parser.error('Cannot use --force with --all. You must specify a list of '
                 'packages you want to force uprev.')

  options.Freeze()
  return options


def main(argv):
  options = _ParseArguments(argv)

  if not options.boards:
    overlays = portage_util.FindOverlays(options.overlay_type)
  else:
    overlays = set()
    for board in options.boards:
      board_overlays = portage_util.FindOverlays(options.overlay_type,
                                                 board=board)
      overlays = overlays.union(board_overlays)

    overlays = list(overlays)

  manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)

  _WorkOnCommit(options, overlays, manifest, options.packages or None)


def CleanStalePackages(boards, package_atoms, chroot):
  """Cleans up stale package info from a previous build.

  Args:
    boards: Boards to clean the packages from.
    package_atoms: A list of package atoms to unmerge.
    chroot (str): The chroot path.
  """
  if package_atoms:
    logging.info('Cleaning up stale packages %s.', package_atoms)

  # First unmerge all the packages for a board, then eclean it.
  # We need these two steps to run in order (unmerge/eclean),
  # but we can let all the boards run in parallel.
  def _CleanStalePackages(board):
    if board:
      suffix = '-' + board
      runcmd = cros_build_lib.RunCommand
    else:
      suffix = ''
      runcmd = cros_build_lib.SudoRunCommand

    chroot_args = ['--chroot', chroot] if chroot else None
    emerge, eclean = 'emerge' + suffix, 'eclean' + suffix
    if not osutils.FindMissingBinaries([emerge, eclean]):
      if package_atoms:
        # If nothing was found to be unmerged, emerge will exit(1).
        result = runcmd([emerge, '-q', '--unmerge'] + list(package_atoms),
                        enter_chroot=True, chroot_args=chroot_args,
                        extra_env={'CLEAN_DELAY': '0'},
                        error_code_ok=True, cwd=constants.SOURCE_ROOT)
        if result.returncode not in (0, 1):
          raise cros_build_lib.RunCommandError('unexpected error', result)
      runcmd([eclean, '-d', 'packages'],
             cwd=constants.SOURCE_ROOT, enter_chroot=True,
             chroot_args=chroot_args, redirect_stdout=True,
             redirect_stderr=True)

  tasks = []
  for board in boards:
    tasks.append([board])
  tasks.append([None])

  parallel.RunTasksInProcessPool(_CleanStalePackages, tasks)


def _WorkOnCommit(options, overlays, manifest, package_list):
  """Commit uprevs of overlays belonging to different git projects in parallel.

  Args:
    options: The options object returned by the argument parser.
    overlays: A list of overlays to work on.
    manifest: The manifest of the given source root.
    package_list: A list of packages passed from commandline to work on.
  """
  overlay_ebuilds = _GetOverlayToEbuildsMap(overlays, package_list,
                                            options.force)

  with parallel.Manager() as manager:
    # Contains the array of packages we actually revved.
    revved_packages = manager.list()
    new_package_atoms = manager.list()
    new_ebuild_files = manager.list()
    removed_ebuild_files = manager.list()

    inputs = [[manifest, [overlay], overlay_ebuilds, revved_packages,
               new_package_atoms, new_ebuild_files, removed_ebuild_files]
              for overlay in overlays]
    parallel.RunTasksInProcessPool(_UprevOverlays, inputs)

    if options.chroot and os.path.exists(options.chroot):
      CleanStalePackages(options.boards or [], new_package_atoms,
                         options.chroot)

    if options.dump_files:
      osutils.WriteFile('/tmp/revved_packages', '\n'.join(revved_packages))
      osutils.WriteFile('/tmp/new_ebuild_files', '\n'.join(new_ebuild_files))
      osutils.WriteFile('/tmp/removed_ebuild_files',
                        '\n'.join(removed_ebuild_files))


def _GetOverlayToEbuildsMap(overlays, package_list, force):
  """Get ebuilds for overlays.

  Args:
    overlays: A list of overlays to work on.
    package_list: A list of packages passed from commandline to work on.
    force (bool): Whether to use packages even if in blacklist.

  Returns:
    A dict mapping each overlay to a list of ebuilds belonging to it.
  """
  root_version = manifest_version.VersionInfo.from_repo(constants.SOURCE_ROOT)
  subdir_removal = manifest_version.VersionInfo('10363.0.0')
  require_subdir_support = root_version < subdir_removal
  use_all = not package_list

  overlay_ebuilds = {}
  inputs = [[overlay, use_all, package_list, force, require_subdir_support]
            for overlay in overlays]
  result = parallel.RunTasksInProcessPool(
      portage_util.GetOverlayEBuilds, inputs)
  for idx, ebuilds in enumerate(result):
    overlay_ebuilds[overlays[idx]] = ebuilds

  return overlay_ebuilds


def _UprevOverlays(manifest, overlays, overlay_ebuilds,
                   revved_packages, new_package_atoms, new_ebuild_files,
                   removed_ebuild_files):
  """Execute uprevs for overlays in sequence.

  Args:
    manifest: The manifest of the given source root.
    overlays: A list over overlays to commit.
    overlay_ebuilds: A dict mapping overlays to their ebuilds.
    revved_packages: A shared list of revved packages.
    new_package_atoms: A shared list of new package atoms.
    new_ebuild_files: New stable ebuild paths.
    removed_ebuild_files: Old ebuild paths that were removed.
  """
  for overlay in overlays:
    if not os.path.isdir(overlay):
      logging.warning('Skipping %s, which is not a directory.', overlay)
      continue

    ebuilds = overlay_ebuilds.get(overlay, [])
    if not ebuilds:
      continue

    with parallel.Manager() as manager:
      # Contains the array of packages we actually revved.
      messages = manager.list()

      inputs = [[overlay, ebuild, manifest, new_ebuild_files,
                 removed_ebuild_files, messages, revved_packages,
                 new_package_atoms] for ebuild in ebuilds]
      parallel.RunTasksInProcessPool(_WorkOnEbuild, inputs)


def _WorkOnEbuild(overlay, ebuild, manifest, new_ebuild_files,
                  removed_ebuild_files, messages, revved_packages,
                  new_package_atoms):
  """Work on a single ebuild.

  Args:
    overlay: The overlay where the ebuild belongs to.
    ebuild: The ebuild to work on.
    manifest: The manifest of the given source root.
    new_ebuild_files: New stable ebuild paths that were created.
    removed_ebuild_files: Old ebuild paths that were removed.
    messages: A share list of commit messages.
    revved_packages: A shared list of revved packages.
    new_package_atoms: A shared list of new package atoms.
  """
  logging.debug('Working on %s, info %s', ebuild.package,
                ebuild.cros_workon_vars)
  try:
    result = ebuild.RevWorkOnEBuild(os.path.join(constants.SOURCE_ROOT, 'src'),
                                    manifest)
  except (OSError, IOError):
    logging.warning(
        'Cannot rev %s\n'
        'Note you will have to go into %s '
        'and reset the git repo yourself.', ebuild.package, overlay)
    raise

  if result:
    new_package, ebuild_path_to_add, ebuild_path_to_remove = result

    if ebuild_path_to_add:
      new_ebuild_files.append(ebuild_path_to_add)
    if ebuild_path_to_remove:
      osutils.SafeUnlink(ebuild_path_to_remove)
      removed_ebuild_files.append(ebuild_path_to_remove)

    messages.append(_GIT_COMMIT_MESSAGE % ebuild.package)
    revved_packages.append(ebuild.package)
    new_package_atoms.append('=%s' % new_package)

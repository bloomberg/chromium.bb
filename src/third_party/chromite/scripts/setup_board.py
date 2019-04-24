# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""setup_board builds the sysroot for a board.

The setup_board process includes the simple directory creations, installs
several configuration files, sets up portage command wrappers and configs,
and installs the toolchain and some core dependency packages (e.g. kernel
headers, gcc-libs).
"""

from __future__ import print_function

from chromite.lib import commandline
from chromite.service import setup_board


def GetParser():
  """Build the argument parser."""
  # TODO(saklein) Remove underscore separated arguments after 2019-06-01
  # (crbug.com/922144).
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board', required=True,
                      help='The name of the board to set up.')
  parser.add_argument('--default', action='store_true', default=False,
                      help='Set the board to the default board in your chroot.')
  parser.add_argument('--force', action='store_true', default=False,
                      help='Force re-creating the board root.')
  # The positive and negative versions of the arguments are used.
  # TODO(saklein) Simplify usages to a single version of the argument.
  parser.add_argument('--usepkg', action='store_true', default=True,
                      dest='usepkg', help='Use binary packages to bootstrap.')
  parser.add_argument('--nousepkg', action='store_false', default=True,
                      dest='usepkg', help='Use binary packages to bootstrap.')

  advanced = parser.add_argument_group('Advanced Options')
  advanced.add_argument('--accept-licenses', '--accept_licenses',
                        help='Licenses to append to the accept list.')

  # Build target related arguments.
  target = parser.add_argument_group('Advanced Build Target Options')
  target.add_argument('--profile',
                      help='The portage configuration profile to use. Profile '
                           'must be located in overlay-board/profiles.')
  target.add_argument('--variant', help='Board variant.')
  target.add_argument('--board-root', '--board_root', type='path',
                      help='Board root.')

  # Arguments related to the build itself.
  build = parser.add_argument_group('Advanced Build Modification Options')
  build.add_argument('--jobs', type=int,
                     help='Maximum number of packages to build in parallel.')
  build.add_argument('--regen-configs', '--regen_configs', action='store_true',
                     default=False,
                     help='Regenerate all config files (useful for '
                          'modifying profiles w/out rebuild).')
  build.add_argument('--quiet', action='store_true', default=False,
                     help="Don't print warnings when board already exists.")
  build.add_argument('--skip-toolchain-update', '--skip_toolchain_update',
                     action='store_true', default=False,
                     help="Don't update toolchain automatically.")
  build.add_argument('--skip-chroot-upgrade', '--skip_chroot_upgrade',
                     action='store_true', default=False,
                     help="Don't run the chroot upgrade automatically; "
                          'use with care.')
  build.add_argument('--skip-board-pkg-init', '--skip_board_pkg_init',
                     action='store_true', default=False,
                     help="Don't emerge any packages during setup_board into "
                          'the board root.')
  build.add_argument('--reuse-pkgs-from-local-boards',
                     '--reuse_pkgs_from_local_boards',
                     dest='reuse_local', action='store_true', default=False,
                     help='Bootstrap from local packages instead of remote '
                          'packages.')

  return parser


def _ParseArgs(args):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(args)

  # Translate raw options to config objects.
  opts.board_conf = setup_board.Board(board=opts.board, variant=opts.variant,
                                      board_root=opts.board_root,
                                      profile=opts.profile)

  opts.run_config = setup_board.SetupBoardRunConfig(
      set_default=opts.default, force=opts.force, usepkg=opts.usepkg,
      jobs=opts.jobs, regen_configs=opts.regen_configs, quiet=opts.quiet,
      update_toolchain=not opts.skip_toolchain_update,
      upgrade_chroot=not opts.skip_chroot_upgrade,
      init_board_pkgs=not opts.skip_board_pkg_init,
      local_build=opts.reuse_local)

  opts.Freeze()
  return opts


def main(argv):
  opts = _ParseArgs(argv)
  setup_board.SetupBoard(opts.board_conf, opts.accept_licenses, opts.run_config)

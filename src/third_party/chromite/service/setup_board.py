# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functionality to setup a board sysroot."""

# TODO(saklein) Fold this functionality into an appropriate location when the
# corresponding API endpoint(s) is built out.

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import sysroot_lib
from chromite.lib import workon_helper


class Error(Exception):
  """Base error class for the module."""


class InvalidArgumentsError(Error):
  """Invalid arguments."""


class NotInChrootError(Error):
  """When SetupBoard is run outside of the chroot."""


# TODO(saklein) Similar to both cros_choose_profile.Board and
# upload_prebuilts.BuildTarget (both in scripts). Find if there are other
# instances of similar classes, and build out a single, shared implementation
# in a more appropriate location.
class Board(object):
  """Manage the board arguments and configs."""

  def __init__(self, board=None, variant=None, board_root=None, profile=None):
    """Board constructor.

    At least board or board_root must be provided.

    Args:
      board (str): The board name.
      variant (str): The variant name.
      board_root (str): The boards fully qualified build directory path.
      profile (str): The name of the profile to be chosen.
    """
    if not board and not board_root:
      # Enforce preconditions.
      raise InvalidArgumentsError('Either board or board_root must be '
                                  'provided.')
    elif board:
      # The board and variant can be specified separately, or can both be
      # contained in the board name, separated by an underscore.
      board_split = board.split('_')
      variant_default = variant
      board_root = os.path.normpath(board_root) if board_root else None
    else:
      board_root = os.path.normpath(board_root)
      board_split = os.path.basename(board_root).split('_')
      variant_default = None

    self.board = board_split.pop(0)
    self.variant = board_split.pop(0) if board_split else variant_default
    self.profile = profile

    if self.variant:
      self.board_variant = '%s_%s' % (self.board, self.variant)
    else:
      self.board_variant = self.board

    self.root = (board_root if board_root else
                 cros_build_lib.GetSysroot(self.board_variant))


class SetupBoardRunConfig(object):
  """Value object for setup board run configurations."""

  def __init__(self, set_default=False, force=False, usepkg=True, jobs=None,
               regen_configs=False, quiet=False, update_toolchain=True,
               upgrade_chroot=True, init_board_pkgs=True, local_build=False):
    """Initialize method.

    Args:
      set_default (bool): Whether to set the passed board as the default.
      force (bool): Force a new sysroot creation when it already exists.
      usepkg (bool): Whether to use binary packages to bootstrap.
      jobs (int): Max number of simultaneous packages to build.
      regen_configs (bool): Whether to only regen the configs.
      quiet (bool): Whether to print notification when sysroot exists.
      update_toolchain (bool): Update the toolchain?
      upgrade_chroot (bool): Upgrade the chroot before building?
      init_board_pkgs (bool): Emerging packages to sysroot?
      local_build (bool): Bootstrap only from local packages?
    """
    self.set_default = set_default
    self.force = force
    self.usepkg = usepkg
    self.jobs = jobs
    self.regen_configs = regen_configs
    self.quiet = quiet
    self.update_toolchain = update_toolchain
    self.update_chroot = upgrade_chroot
    self.init_board_pkgs = init_board_pkgs
    self.local_build = local_build

  def UpdateChrootArgs(self):
    """Create a list containing the relevant update_chroot arguments.

    Returns:
      list[str]
    """
    args = []
    if self.usepkg:
      args += ['--usepkg']
    else:
      args += ['--nousepkg']

    if self.jobs > 0:
      args += ['--jobs', str(self.jobs)]

    if not self.update_toolchain:
      args += ['--skip_toolchain_update']

    return args


def SetupBoard(board, accept_licenses=None, run_configs=None):
  """Setup a board's sysroot.

  Args:
    board (Board): The board configuration.
    accept_licenses (str|None): A list of additional licenses to accept.
    run_configs (SetupBoardRunConfig): The run configs.
  """
  if not cros_build_lib.IsInsideChroot():
    # TODO(saklein) switch to build out command and run inside chroot.
    raise NotInChrootError('SetupBoard must be run from inside the chroot')

  sysroot = sysroot_lib.Sysroot(board.root)
  if sysroot.Exists() and not run_configs.force and not run_configs.quiet:
    logging.warning('Board output directory already exists: %s\n'
                    'Use --force to clobber the board root and start again.',
                    sysroot.path)

  # Make sure we have valid run configs setup.
  run_configs = run_configs or SetupBoardRunConfig()
  # Override regen_configs setting to force full setup run if the sysroot does
  # not exist.
  run_configs.regen_configs = run_configs.regen_configs and sysroot.Exists()

  # Make sure the chroot is fully up to date before we start unless the
  # chroot update is explicitly disabled.
  if run_configs.update_chroot:
    update_chroot = [os.path.join(constants.CROSUTILS_DIR, 'update_chroot'),
                     '--toolchain_boards', board.board]
    update_chroot += run_configs.UpdateChrootArgs()
    cros_build_lib.RunCommand(update_chroot)

  # Delete old sysroot to force a fresh start if requested.
  if sysroot.Exists() and run_configs.force:
    sysroot.Delete(async=True)

  # Step 1: Create folders.
  # Dependencies: None.
  # Create the skeleton.
  logging.info('Creating sysroot directories.')
  CreateSysrootSkeleton(sysroot)

  # Step 2: Standalone configurations.
  # Dependencies: Folders exist.
  # Install main, board setup, and user make.conf files.
  logging.info('Installing configurations into sysroot.')
  InstallConfigs(sysroot, board)

  # Step 3: Portage configurations.
  # Dependencies: make.conf.board_setup.
  # Create the command wrappers, choose profile, and make.conf.board.
  # Refresh the workon symlinks to compensate for crbug.com/679831.
  logging.info('Setting up portage in the sysroot.')
  InstallPortageConfigs(sysroot, board, accept_licenses,
                        run_configs.local_build)

  # Developer Experience Step: Set default board (if requested) to allow running
  # later commands without needing to pass the --board argument.
  if run_configs.set_default:
    cros_build_lib.SetDefaultBoard(board.board_variant)

  if run_configs.regen_configs:
    # We're now done if we're only regenerating the configs.
    return

  # Step 4: Install toolchain and packages.
  # Dependencies: Portage configs and wrappers have been installed.
  if run_configs.init_board_pkgs:
    logging.info('Updating toolchain.')
    # Use the local packages if we're doing a local only build or usepkg is set.
    local_init = run_configs.usepkg or run_configs.local_build
    InstallToolchain(sysroot, board, local_init=local_init)


def CreateSysrootSkeleton(sysroot):
  """Create the sysroot skeleton.

  Dependencies: None.
  Creates the sysroot directory structure and installs the portage hooks.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
  """
  sysroot.CreateSkeleton()


def InstallConfigs(sysroot, board):
  """Install standalone configuration files into the sysroot.

  Dependencies: The sysroot exists (i.e. CreateSysrootSkeleton).
  Installs the main, board setup, and user make.conf files.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
    board (Board): The board being setup in the sysroot.
  """
  sysroot.InstallMakeConf()
  sysroot.InstallMakeConfBoardSetup(board.board_variant)
  sysroot.InstallMakeConfUser()


def InstallPortageConfigs(sysroot, board, accept_licenses, local_build):
  """Install portage wrappers and configurations.

  Dependencies: make.conf.board_setup (InstallConfigs).
  Create the command wrappers, choose profile, and generate make.conf.board.
  Refresh the workon symlinks to compensate for crbug.com/679831.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
    board (Board): The board being installed in the sysroot.
    accept_licenses (str): Additional accepted licenses as a string.
    local_build (bool): If the build is a local only build.
  """
  sysroot.CreateAllWrappers(friendly_name=board.board_variant)
  _ChooseProfile(board, sysroot)
  _RefreshWorkonSymlinks(board.board_variant, sysroot)
  # Must be done after the profile is chosen or binhosts may be incomplete.
  sysroot.InstallMakeConfBoard(accepted_licenses=accept_licenses,
                               local_only=local_build)


def InstallToolchain(sysroot, board, local_init=True):
  """Install toolchain and packages.

  Dependencies: Portage configs and wrappers have been installed
    (InstallPortageConfigs).
  Install the toolchain and the implicit dependencies.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot to install to.
    board (Board): The board whose toolchain is being installed.
    local_init (bool): Whether to use local packages to bootstrap implicit
      dependencies.
  """
  sysroot.UpdateToolchain(board.board_variant, local_init=local_init)


def _RefreshWorkonSymlinks(board, sysroot):
  """Force refresh the workon symlinks.

  Create an instance of the WorkonHelper, which will recreate all symlinks
  to masked/unmasked packages currently worked on in case the sysroot was
  recreated (crbug.com/679831).

  This was done with a call to `cros_workon list` in the bash version of
  the script, but all we actually need is for the WorkonHelper to be
  instantiated since it refreshes the symlinks in its __init__.

  Args:
    board (str): The full board name.
    sysroot (sysroot_lib.Sysroot): The board's sysroot.
  """
  workon_helper.WorkonHelper(sysroot.path, friendly_name=board)


def _ChooseProfile(board, sysroot):
  """Helper function to execute cros_choose_profile.

  TODO(saklein) Refactor cros_choose_profile to avoid needing the RunCommand
  call here, and by extension this method all together.

  Args:
    board (Board): The board config for the board whose profile is being chosen.
    sysroot (sysroot_lib.Sysroot): The sysroot for which the profile is
      being chosen.
  """
  choose_profile = ['cros_choose_profile', '--board', board.board_variant,
                    '--board-root', sysroot.path]
  if board.profile:
    # Chooses base by default, only override when we have a passed param.
    choose_profile += ['--profile', board.profile]
  try:
    cros_build_lib.RunCommand(choose_profile, print_cmd=False)
  except cros_build_lib.RunCommandError as e:
    logging.error('Selecting profile failed, removing incomplete board '
                  'directory!')
    sysroot.Delete()
    raise e

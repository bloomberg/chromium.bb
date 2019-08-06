# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot service."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.lib import workon_helper


class Error(Exception):
  """Base error class for the module."""


class InvalidArgumentsError(Error):
  """Invalid arguments."""


class NotInChrootError(Error):
  """When SetupBoard is run outside of the chroot."""


class UpdateChrootError(Error):
  """Error occurred when running update chroot."""


class SetupBoardRunConfig(object):
  """Value object for full setup board run configurations."""

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

  def GetUpdateChrootArgs(self):
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


class BuildPackagesRunConfig(object):
  """Value object to hold build packages run configs."""

  def __init__(self, event_file=None, usepkg=True, install_debug_symbols=False,
               packages=None, use_flags=None):
    """Init method.

    Args:
      event_file (str): The event file location, enables events.
      usepkg (bool): Whether to use binpkgs or build from source. False
        currently triggers a local build, which will enable local reuse.
      install_debug_symbols (bool): Whether to include the debug symbols for all
        packages.
      packages (list[str]|None): The list of packages to install, by default
        install all packages for the target.
      use_flags (list[str]|None): A list of use flags to set.
    """
    self.event_file = event_file
    self.usepkg = usepkg
    self.install_debug_symbols = install_debug_symbols
    self.packages = packages
    self.use_flags = use_flags

  def GetBuildPackagesArgs(self):
    """Get the build_packages script arguments."""
    # Defaults for the builder.
    # TODO(saklein): Parametrize/rework the defaults when build_packages is
    #   ported to chromite.
    args = ['--accept_licenses', '@CHROMEOS', '--skip_chroot_upgrade']

    if self.event_file:
      args.append('--withevents')
      args.extend(['--eventfile', self.event_file])

    if not self.usepkg:
      args.append('--nousepkg')
      args.append('--reuse_pkgs_from_local_boards')

    if self.install_debug_symbols:
      args.append('--withdebugsymbols')

    if self.packages:
      args.extend(self.packages)

    return args

  def HasUseFlags(self):
    """Check if we have use flags."""
    return len(self.use_flags) > 0

  def GetUseFlags(self):
    """Get the use flags as a single string."""
    use_flags = self.use_flags
    if use_flags:
      # We have use flags to set, but we need to append them to any existing
      # use flags rather than overwrite them completely.
      # TODO(saklein) Add config for whether to extend or overwrite?
      existing_flags = os.environ.get('USE', '').split()
      existing_flags.extend(use_flags)
      use_flags = existing_flags

      return ' '.join(use_flags)

    return None

def SetupBoard(target, accept_licenses=None, run_configs=None):
  """Run the full process to setup a board's sysroot.

  This is the entry point to run the setup_board script.

  Args:
    target (build_target_util.BuildTarget): The build target configuration.
    accept_licenses (str|None): The additional licenses to accept.
    run_configs (SetupBoardRunConfig): The run configs.

  Raises:
    sysroot_lib.ToolchainInstallError when the toolchain fails to install.
  """
  if not cros_build_lib.IsInsideChroot():
    # TODO(saklein) switch to build out command and run inside chroot.
    raise NotInChrootError('SetupBoard must be run from inside the chroot')

  # Make sure we have valid run configs setup.
  run_configs = run_configs or SetupBoardRunConfig()

  sysroot = Create(target, run_configs, accept_licenses)

  if run_configs.regen_configs:
    # We're now done if we're only regenerating the configs.
    return

  InstallToolchain(target, sysroot, run_configs)


def Create(target, run_configs, accept_licenses):
  """Create a sysroot.

  This entry point is the subset of the full setup process that does the
  creation and configuration of a sysroot, including installing portage.

  Args:
    target (build_target.BuildTarget): The build target being installed in the
      sysroot being created.
    run_configs (SetupBoardRunConfig): The run configs.
    accept_licenses (str|None): The additional licenses to accept.
  """
  cros_build_lib.AssertInsideChroot()

  sysroot = sysroot_lib.Sysroot(target.root)

  if sysroot.Exists() and not run_configs.force and not run_configs.quiet:
    logging.warning('Board output directory already exists: %s\n'
                    'Use --force to clobber the board root and start again.',
                    sysroot.path)

  # Override regen_configs setting to force full setup run if the sysroot does
  # not exist.
  run_configs.regen_configs = run_configs.regen_configs and sysroot.Exists()

  # Make sure the chroot is fully up to date before we start unless the
  # chroot update is explicitly disabled.
  if run_configs.update_chroot:
    logging.info('Updating chroot.')
    update_chroot = [os.path.join(constants.CROSUTILS_DIR, 'update_chroot'),
                     '--toolchain_boards', target.name]
    update_chroot += run_configs.GetUpdateChrootArgs()
    try:
      cros_build_lib.RunCommand(update_chroot)
    except cros_build_lib.RunCommandError:
      raise UpdateChrootError('Error occurred while updating the chroot.'
                              'See the logs for more information.')

  # Delete old sysroot to force a fresh start if requested.
  if sysroot.Exists() and run_configs.force:
    sysroot.Delete(async=True)

  # Step 1: Create folders.
  # Dependencies: None.
  # Create the skeleton.
  logging.info('Creating sysroot directories.')
  _CreateSysrootSkeleton(sysroot)

  # Step 2: Standalone configurations.
  # Dependencies: Folders exist.
  # Install main, board setup, and user make.conf files.
  logging.info('Installing configurations into sysroot.')
  _InstallConfigs(sysroot, target)

  # Step 3: Portage configurations.
  # Dependencies: make.conf.board_setup.
  # Create the command wrappers, choose profile, and make.conf.board.
  # Refresh the workon symlinks to compensate for crbug.com/679831.
  logging.info('Setting up portage in the sysroot.')
  _InstallPortageConfigs(sysroot, target, accept_licenses,
                         run_configs.local_build)

  # Developer Experience Step: Set default board (if requested) to allow
  # running later commands without needing to pass the --board argument.
  if run_configs.set_default:
    cros_build_lib.SetDefaultBoard(target.name)

  return sysroot


def InstallToolchain(target, sysroot, run_configs):
  """Update the toolchain to a sysroot.

  This entry point just installs the target's toolchain into the sysroot.
  Everything else must have been done already for this to be successful.

  Args:
    target (build_target_util.BuildTarget): The target whose toolchain is being
      installed.
    sysroot (sysroot_lib.Sysroot): The sysroot where the toolchain is being
      installed.
    run_configs (SetupBoardRunConfig): The run configs.
  """
  cros_build_lib.AssertInsideChroot()
  if not sysroot.Exists():
    # Sanity check before we try installing anything.
    raise ValueError('The sysroot must exist, run Create first.')

  # Step 4: Install toolchain and packages.
  # Dependencies: Portage configs and wrappers have been installed.
  if run_configs.init_board_pkgs:
    logging.info('Updating toolchain.')
    # Use the local packages if we're doing a local only build or usepkg is set.
    local_init = run_configs.usepkg or run_configs.local_build
    _InstallToolchain(sysroot, target, local_init=local_init)


def BuildPackages(target, sysroot, run_configs):
  """Build and install packages into a sysroot.

  Args:
    target (build_target_util.BuildTarget): The target whose packages are being
      installed.
    sysroot (sysroot_lib.Sysroot): The sysroot where the packages are being
      installed.
    run_configs (BuildPackagesRunConfig): The run configs.
  """
  cros_build_lib.AssertInsideChroot()

  cmd = [os.path.join(constants.CROSUTILS_DIR, 'build_packages'),
         '--board', target.name, '--board_root', sysroot.path]
  cmd += run_configs.GetBuildPackagesArgs()

  with osutils.TempDir(base_dir='/tmp') as tempdir:
    extra_env = {
        constants.CROS_METRICS_DIR_ENVVAR: tempdir,
        'USE_NEW_PARALLEL_EMERGE': '1',
    }

    if run_configs.use_flags:
      extra_env['USE'] = run_configs.GetUseFlags()

    try:
      cros_build_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env)
    except cros_build_lib.RunCommandError as e:
      failed_pkgs = portage_util.ParseDieHookStatusFile(tempdir)
      raise sysroot_lib.PackageInstallError(
          e.message, e.result, exception=e, packages=failed_pkgs)


def _CreateSysrootSkeleton(sysroot):
  """Create the sysroot skeleton.

  Dependencies: None.
  Creates the sysroot directory structure and installs the portage hooks.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
  """
  sysroot.CreateSkeleton()


def _InstallConfigs(sysroot, target):
  """Install standalone configuration files into the sysroot.

  Dependencies: The sysroot exists (i.e. CreateSysrootSkeleton).
  Installs the main, board setup, and user make.conf files.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
    target (build_target.BuildTarget): The build target being setup in
      the sysroot.
  """
  sysroot.InstallMakeConf()
  sysroot.InstallMakeConfBoardSetup(target.name)
  sysroot.InstallMakeConfUser()


def _InstallPortageConfigs(sysroot, target, accept_licenses, local_build):
  """Install portage wrappers and configurations.

  Dependencies: make.conf.board_setup (InstallConfigs).
  Create the command wrappers, choose profile, and generate make.conf.board.
  Refresh the workon symlinks to compensate for crbug.com/679831.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot.
    target (build_target.BuildTarget): The build target being installed
      in the sysroot.
    accept_licenses (str): Additional accepted licenses as a string.
    local_build (bool): If the build is a local only build.
  """
  sysroot.CreateAllWrappers(friendly_name=target.name)
  _ChooseProfile(target, sysroot)
  _RefreshWorkonSymlinks(target.name, sysroot)
  # Must be done after the profile is chosen or binhosts may be incomplete.
  sysroot.InstallMakeConfBoard(accepted_licenses=accept_licenses,
                               local_only=local_build)


def _InstallToolchain(sysroot, target, local_init=True):
  """Install toolchain and packages.

  Dependencies: Portage configs and wrappers have been installed
    (InstallPortageConfigs).
  Install the toolchain and the implicit dependencies.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot to install to.
    target (build_target.BuildTarget): The build target whose toolchain is
      being installed.
    local_init (bool): Whether to use local packages to bootstrap implicit
      dependencies.
  """
  sysroot.UpdateToolchain(target.name, local_init=local_init)


def _RefreshWorkonSymlinks(target, sysroot):
  """Force refresh the workon symlinks.

  Create an instance of the WorkonHelper, which will recreate all symlinks
  to masked/unmasked packages currently worked on in case the sysroot was
  recreated (crbug.com/679831).

  This was done with a call to `cros_workon list` in the bash version of
  the script, but all we actually need is for the WorkonHelper to be
  instantiated since it refreshes the symlinks in its __init__.

  Args:
    target (str): The build target name.
    sysroot (sysroot_lib.Sysroot): The board's sysroot.
  """
  workon_helper.WorkonHelper(sysroot.path, friendly_name=target)


def _ChooseProfile(target, sysroot):
  """Helper function to execute cros_choose_profile.

  TODO(saklein) Refactor cros_choose_profile to avoid needing the RunCommand
  call here, and by extension this method all together.

  Args:
    target (build_target_util.BuildTarget): The build target whose profile is
      being chosen.
    sysroot (sysroot_lib.Sysroot): The sysroot for which the profile is
      being chosen.
  """
  choose_profile = ['cros_choose_profile', '--board', target.name,
                    '--board-root', sysroot.path]
  if target.profile:
    # Chooses base by default, only override when we have a passed param.
    choose_profile += ['--profile', target.profile]
  try:
    cros_build_lib.RunCommand(choose_profile, print_cmd=False)
  except cros_build_lib.RunCommandError as e:
    logging.error('Selecting profile failed, removing incomplete board '
                  'directory!')
    sysroot.Delete()
    raise e

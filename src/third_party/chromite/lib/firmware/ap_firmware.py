# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""AP firmware utilities."""

from __future__ import print_function

import collections
import importlib

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import workon_helper
from chromite.lib.firmware import flash_ap

_BUILD_TARGET_CONFIG_MODULE = 'chromite.lib.firmware.ap_firmware_config.%s'
_CONFIG_BUILD_WORKON_PACKAGES = 'BUILD_WORKON_PACKAGES'
_CONFIG_BUILD_PACKAGES = 'BUILD_PACKAGES'

# The build configs. The workon and build fields both contain tuples of
# packages.
BuildConfig = collections.namedtuple('BuildConfig', ('workon', 'build'))

# The set of commands for a servo deploy.
ServoDeployCommands = collections.namedtuple('ServoDeployCommands',
                                             ('dut_on', 'dut_off', 'flash'))


class Error(Exception):
  """Base error class for the module."""


class BuildError(Error):
  """Failure in the build command."""


class BuildTargetNotConfiguredError(Error):
  """Thrown when a config module does not exist for the build target."""


class DeployError(Error):
  """Failure in the deploy command."""


class InvalidConfigError(Error):
  """The config does not contain the required information for the operation."""


def build(build_target, fw_name=None, dry_run=False):
  """Build the AP Firmware.

  Args:
    build_target (BuildTarget): The build target (board) being built.
    fw_name (str|None): Optionally set the FW_NAME envvar to allow building
      the firmware for only a specific variant.
    dry_run (bool): Whether to perform a dry run.
  """
  logging.notice('Building AP Firmware.')

  workon = workon_helper.WorkonHelper(build_target.root, build_target.name)
  config = _get_build_config(build_target)

  # Workon the required packages. Also calculate the list of packages that need
  # to be stopped to clean up after ourselves when we're done.
  if config.workon:
    before_workon = workon.ListAtoms()
    logging.info('cros_workon starting packages.')
    logging.debug('cros_workon-%s start %s', build_target.name,
                  ' '.join(config.workon))

    if dry_run:
      # Pretend it worked: after = before U workon.
      after_workon = set(before_workon) & set(config.workon)
    else:
      workon.StartWorkingOnPackages(config.workon)
      after_workon = workon.ListAtoms()

    # Stop = the set we actually started. Preserves workon started status for
    # any in the config.workon packages that were already worked on.
    stop_packages = list(set(after_workon) - set(before_workon))
  else:
    stop_packages = []

  extra_env = {'FW_NAME': fw_name} if fw_name else None
  # Run the emerge command to build the packages. Don't raise an exception here
  # if it fails so we can cros workon stop afterwords.
  logging.info('Building the AP firmware packages.')
  # Print command with --debug.
  print_cmd = logging.getLogger(__name__).getEffectiveLevel() == logging.DEBUG
  result = cros_build_lib.run(
      [build_target.get_command('emerge')] + list(config.build),
      print_cmd=print_cmd,
      check=False,
      debug_level=logging.DEBUG,
      dryrun=dry_run,
      extra_env=extra_env)

  # Reset the environment.
  logging.notice('Restoring cros_workon status.')
  if stop_packages:
    # Stop the packages we started.
    logging.info('Stopping workon packages previously started.')
    logging.debug('cros_workon-%s stop %s', build_target.name,
                  ' '.join(stop_packages))
    if not dry_run:
      workon.StopWorkingOnPackages(stop_packages)
  else:
    logging.info('No packages needed to be stopped.')

  if result.returncode:
    # Now raise the emerge failure since we're done cleaning up.
    raise BuildError('The emerge command failed. Run with --verbose or --debug '
                     'to see the emerge output for details.')

  logging.notice('Done! AP firmware successfully built.')

def deploy(build_target,
           image,
           device,
           flashrom=False,
           fast=False,
           verbose=False,
           dryrun=False):
  """Deploy a firmware image to a device.

  Args:
    build_target (build_target_lib.BuildTarget): The build target.
    image (str): The path to the image to flash.
    device (commandline.Device): The DUT being flashed.
    flashrom (bool): Whether to use flashrom or futility.
    fast (bool): Perform a faster flash that isn't validated.
    verbose (bool): Whether to enable verbose output of the flash commands.
    dryrun (bool): Whether to actually execute the deployment or just print the
      operations that would have been performed.
  """
  try:
    flash_ap.deploy(
        build_target=build_target,
        image=image,
        device=device,
        flashrom=flashrom,
        fast=fast,
        verbose=verbose,
        dryrun=dryrun)
  except flash_ap.Error as e:
    # Reraise as a DeployError for future compatibility.
    raise DeployError(str(e))


class DeployConfig(object):
  """Deploy configuration wrapper."""

  FORCE_FLASHROM = 'flashrom'
  FORCE_FUTILITY = 'futility'

  def __init__(self,
               get_commands,
               force_fast=None,
               servo_force_command=None,
               ssh_force_command=None):
    """DeployConfig init.

    Args:
      get_commands: A function that takes a servo and returns four sets of
        commands: The dut on, dut off, flashrom, and futility commands to flash
        a servo for a particular build target.
      force_fast: A function that takes two arguments; a bool to indicate if it
        is for a futility (True) or flashrom (False) command.
      servo_force_command: One of the FORCE_{command} constants to force use of
        a specific command, or None to not force.
      ssh_force_command: One of the FORCE_{command} constants to force use of
        a specific command, or None to not force.
    """
    self._get_commands = get_commands
    self._force_fast = force_fast
    self._servo_force_command = servo_force_command
    self._ssh_force_command = ssh_force_command

  @property
  def servo_force_flashrom(self):
    return self._servo_force_command == self.FORCE_FLASHROM

  @property
  def servo_force_futility(self):
    return self._servo_force_command == self.FORCE_FUTILITY

  @property
  def ssh_force_flashrom(self):
    return self._ssh_force_command == self.FORCE_FLASHROM

  @property
  def ssh_force_futility(self):
    return self._ssh_force_command == self.FORCE_FUTILITY

  def force_fast(self, servo, flashrom):
    """Check if the fast flash option is required.

    Some configurations fail flash verification, which can be skipped with
    a fast flash.

    Args:
      servo (servo_lib.Servo): The servo connected to the DUT.
      flashrom (bool): Whether flashrom is being used instead of futility.

    Returns:
      bool: True if it requires a fast flash, False otherwise.
    """
    if not self._force_fast:
      # No function defined in the module, so no required cases.
      return False

    return self._force_fast(not flashrom, servo)

  def get_servo_commands(self,
                         servo,
                         image_path,
                         flashrom=False,
                         fast=False,
                         verbose=False):
    """Get the servo flash commands from the build target config."""
    dut_on, dut_off, flashrom_cmd, futility_cmd = self._get_commands(servo)

    # Make any forced changes to the given options.
    if not flashrom and self.servo_force_flashrom:
      logging.notice('Forcing flashrom flash.')
      flashrom = True
    elif flashrom and self.servo_force_futility:
      logging.notice('Forcing futility flash.')
      flashrom = False

    if not fast and self.force_fast(servo, flashrom):
      logging.notice('Forcing fast flash.')
      fast = True

    # Make common command additions here to simplify the config modules.
    flashrom_cmd += [image_path]
    futility_cmd += [image_path]
    futility_cmd += ['--force', '--wp=0']

    if fast:
      flashrom_cmd += ['-n']
      futility_cmd += ['--fast']
    if verbose:
      flashrom_cmd += ['-V']
      futility_cmd += ['-v']

    return ServoDeployCommands(
        dut_on=dut_on,
        dut_off=dut_off,
        flash=flashrom_cmd if flashrom else futility_cmd)


def _get_build_config(build_target):
  """Get the relevant build config for |build_target|."""
  module = _get_config_module(build_target)
  workon_pkgs = getattr(module, _CONFIG_BUILD_WORKON_PACKAGES, None)
  build_pkgs = getattr(module, _CONFIG_BUILD_PACKAGES, None)

  if not build_pkgs:
    raise InvalidConfigError(
        'No packages specified to build in the configs for %s. Check the '
        'configs in chromite/lib/firmware/ap_firmware_config/%s.py.' %
        (build_target.name, build_target.name))

  return BuildConfig(workon=workon_pkgs, build=build_pkgs)


def _get_deploy_config(build_target):
  """Get the relevant deploy config for |build_target|."""
  module = _get_config_module(build_target)

  # Get the force fast function if available.
  force_fast = getattr(module, 'is_fast_required', None)

  # Check the force servo command options.
  servo_force = None
  if getattr(module, 'DEPLOY_SERVO_FORCE_FLASHROM', False):
    servo_force = DeployConfig.FORCE_FLASHROM
  elif getattr(module, 'DEPLOY_SERVO_FORCE_FUTILITY', False):
    servo_force = DeployConfig.FORCE_FUTILITY

  # Check the force SSH command options.
  ssh_force = None
  if getattr(module, 'DEPLOY_SSH_FORCE_FLASHROM', False):
    ssh_force = DeployConfig.FORCE_FLASHROM
  elif getattr(module, 'DEPLOY_SSH_FORCE_FUTILITY', False):
    ssh_force = DeployConfig.FORCE_FUTILITY

  return DeployConfig(
      module.get_commands,
      force_fast=force_fast,
      servo_force_command=servo_force,
      ssh_force_command=ssh_force)


def _get_config_module(build_target):
  """Get the |build_target|'s config module."""
  name = _BUILD_TARGET_CONFIG_MODULE % build_target.name
  try:
    return importlib.import_module(name)
  except ImportError:
    raise BuildTargetNotConfiguredError(
        'Could not find a config module for %s.' % build_target.name)

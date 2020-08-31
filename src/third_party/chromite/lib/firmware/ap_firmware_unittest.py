# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the ap_firmware module."""

from __future__ import print_function

import mock

from chromite.lib import build_target_lib
from chromite.lib import cros_test_lib
from chromite.lib import workon_helper
from chromite.lib.firmware import ap_firmware
from chromite.lib.firmware import servo_lib


class BuildTest(cros_test_lib.RunCommandTestCase):
  """Tests for building ap firmware."""

  def test_valid_build_config(self):
    """Test building of the build config object."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=('pkg1', 'pkg2'), BUILD_PACKAGES=('pkg3', 'pkg4'))

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    # pylint: disable=protected-access
    build_config = ap_firmware._get_build_config(
        build_target_lib.BuildTarget('board'))

    self.assertEqual(('pkg1', 'pkg2'), build_config.workon)
    self.assertEqual(('pkg3', 'pkg4'), build_config.build)

  def test_no_workon_config(self):
    """Test building of the build config object with no workon packages."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=None, BUILD_PACKAGES=('pkg3', 'pkg4'))

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    # pylint: disable=protected-access
    build_config = ap_firmware._get_build_config(
        build_target_lib.BuildTarget('board'))

    self.assertFalse(build_config.workon)
    self.assertEqual(('pkg3', 'pkg4'), build_config.build)

  def test_invalid_build_config(self):
    """Test invalid build configs."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=('pkg1', 'pkg2'), BUILD_PACKAGES=tuple())

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    with self.assertRaises(ap_firmware.InvalidConfigError):
      # pylint: disable=protected-access
      ap_firmware._get_build_config(build_target_lib.BuildTarget('board'))

  def test_build(self):
    """Sanity checks the workon and command building functions properly."""
    # Note: The workon helper handles looking up full category/package atom
    # when just given package names.
    build_pkgs = ('build1', 'build2')
    workon_pkgs = ('workon1', 'workon2')
    # Inconsequential pkgs + 1 we need.
    existing_workons = ['cat/pkg1', 'cat/pkg2', 'cat/workon1']
    existing_and_required = existing_workons + ['cat/workon2']
    # Should only stop the ones that weren't previously worked on.
    expected_workon_stop = ['cat/workon2']

    build_config = ap_firmware.BuildConfig(workon=workon_pkgs, build=build_pkgs)
    build_target = build_target_lib.BuildTarget('board')

    # Simulate starting the required workon packages. Return first the existing
    # workon packages, then the ones we're starting plus the existing.
    self.PatchObject(
        workon_helper.WorkonHelper,
        'ListAtoms',
        side_effect=[existing_workons, existing_and_required])
    # Start and stop workon patches for verifying calls.
    start_patch = self.PatchObject(workon_helper.WorkonHelper,
                                   'StartWorkingOnPackages')
    stop_patch = self.PatchObject(workon_helper.WorkonHelper,
                                  'StopWorkingOnPackages')

    # Patch in the build config.
    self.PatchObject(
        ap_firmware, '_get_build_config', return_value=build_config)

    ap_firmware.build(build_target, 'board-variant')

    # Verify the workon packages. Should be starting all the required workon
    # packages, but only stopping the ones that we started in the command.
    start_patch.assert_called_once_with(workon_pkgs)
    stop_patch.assert_called_once_with(expected_workon_stop)
    # Verify we try to build all the build packages, and that the FW_NAME envvar
    # has been set.
    self.rc.assertCommandContains(
        list(build_pkgs), extra_env={'FW_NAME': 'board-variant'})


class DeployConfigTest(cros_test_lib.TestCase):
  """Test the deploy configuration class."""

  def setUp(self):
    self.servo = servo_lib.Servo(servo_lib.SERVO_C2D2, 'abc123')

    # Expected dut commands and base flash commands.
    self.expected_dut_on = [['dut_on']]
    self.expected_dut_off = [['dut_off']]
    base_flashrom = ['flashrom']
    base_futility = ['futility']
    # The get commands function returning the base commands.
    commands = (
        self.expected_dut_on[:],
        self.expected_dut_off[:],
        base_flashrom[:],
        base_futility[:],
    )
    self.get_commands = (
        lambda *args: commands
    )

    # The expected commands.
    self.image = 'image'
    self.expected_flashrom = base_flashrom + [self.image]
    self.expected_futility = base_futility + [self.image]
    # The optional fast and verbose arguments.
    self.flashrom_fast_verbose = ['-n', '-V']
    self.futility_fast_verbose = ['--fast', '-v']

  def _assert_command(self, flash, flashrom=False, fast_verbose=None):
    """Helper to check the flash command.

    Args:
      flash (list[str]): The command being checked.
      flashrom (bool): Check flashrom (True) or futility (False).
      fast_verbose (bool|None): Assert the fast and verbose options were (True)
        or were not (False) added to the command, or skip the check (None).
    """
    # Base command checks.
    expected = self.expected_flashrom if flashrom else self.expected_futility
    for element in expected:
      self.assertIn(element, flash)

    # Fast/verbose checks.
    expected = (
        self.flashrom_fast_verbose if flashrom else self.futility_fast_verbose)
    if fast_verbose:
      for element in expected:
        self.assertIn(element, flash)
    elif fast_verbose is False:
      for element in expected:
        self.assertNotIn(element, flash)

  def test_force_fast(self):
    """Test the force fast call-through."""
    get_commands = lambda *args: ([], [], [], [])
    force_fast = lambda futility, servo: futility and servo == self.servo

    config = ap_firmware.DeployConfig(get_commands, force_fast=force_fast)
    self.assertTrue(config.force_fast(flashrom=False, servo=self.servo))
    self.assertFalse(config.force_fast(flashrom=True, servo=self.servo))

  def test_no_force_fast(self):
    """Sanity check no force fast function gets handled properly."""
    get_commands = lambda *args: ([], [], [], [])

    config = ap_firmware.DeployConfig(get_commands)
    self.assertFalse(config.force_fast(flashrom=False, servo=self.servo))
    self.assertFalse(config.force_fast(flashrom=True, servo=self.servo))

  def test_dut_commands(self):
    """Sanity check for the dut commands pass through."""
    config = ap_firmware.DeployConfig(self.get_commands)
    commands = config.get_servo_commands(self.servo, self.image)

    self.assertListEqual(self.expected_dut_on, commands.dut_on)
    self.assertListEqual(self.expected_dut_off, commands.dut_off)

  def test_flashrom_command(self):
    """Test the base flashrom command is built correctly."""
    config = ap_firmware.DeployConfig(self.get_commands)
    commands = config.get_servo_commands(self.servo, self.image, flashrom=True)

    self._assert_command(commands.flash, flashrom=True, fast_verbose=False)

  def test_fast_verbose_flashrom(self):
    """Sanity check the fast/verbose flashrom arguments get added."""
    config = ap_firmware.DeployConfig(self.get_commands)
    commands = config.get_servo_commands(
        self.servo, self.image, flashrom=True, fast=True, verbose=True)

    self._assert_command(commands.flash, flashrom=True, fast_verbose=True)

  def test_futility_command(self):
    """Test the futility command is built correctly."""
    config = ap_firmware.DeployConfig(self.get_commands)
    commands = config.get_servo_commands(self.servo, self.image)

    self._assert_command(commands.flash, flashrom=False, fast_verbose=False)

  def test_fast_verbose_futility(self):
    """Sanity check the fast/verbose futility arguments get added."""
    config = ap_firmware.DeployConfig(self.get_commands)
    commands = config.get_servo_commands(
        self.servo, self.image, fast=True, verbose=True)

    self._assert_command(commands.flash, flashrom=False, fast_verbose=True)

  def test_force_fast_flashrom(self):
    """Test the flashrom and fast command alterations."""
    force_fast = lambda *args: True
    config = ap_firmware.DeployConfig(
        self.get_commands,
        force_fast=force_fast,
        servo_force_command=ap_firmware.DeployConfig.FORCE_FLASHROM)

    commands = config.get_servo_commands(self.servo, self.image, verbose=True)
    self._assert_command(commands.flash, flashrom=True, fast_verbose=True)

  def test_force_fast_futility(self):
    """Test the futility and fast command alterations."""
    force_fast = lambda *args: True
    config = ap_firmware.DeployConfig(
        self.get_commands,
        force_fast=force_fast,
        servo_force_command=ap_firmware.DeployConfig.FORCE_FUTILITY)

    commands = config.get_servo_commands(self.servo, self.image, verbose=True)
    self._assert_command(commands.flash, flashrom=False, fast_verbose=True)

# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot service unittest."""

from __future__ import print_function

import os

from chromite.lib import build_target_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib
from chromite.service import sysroot


class SetupBoardRunConfigTest(cros_test_lib.TestCase):
  """Tests for the SetupBoardRunConfig class."""

  def testGetUpdateChrootArgs(self):
    """Test the update chroot args conversion method."""
    # False/0/None tests.
    instance = sysroot.SetupBoardRunConfig(usepkg=False, jobs=None,
                                           update_toolchain=False)
    args = instance.GetUpdateChrootArgs()
    self.assertIn('--nousepkg', args)
    self.assertIn('--skip_toolchain_update', args)
    self.assertNotIn('--usepkg', args)
    self.assertNotIn('--jobs', args)

    instance.jobs = 0
    args = instance.GetUpdateChrootArgs()
    self.assertNotIn('--jobs', args)

    # True/set values tests.
    instance = sysroot.SetupBoardRunConfig(usepkg=True, jobs=1,
                                           update_toolchain=True)
    args = instance.GetUpdateChrootArgs()
    self.assertIn('--usepkg', args)
    self.assertIn('--jobs', args)
    self.assertNotIn('--nousepkg', args)
    self.assertNotIn('--skip_toolchain_update', args)


class SetupBoardTest(cros_test_lib.MockTestCase):
  """Tests for SetupBoard."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

  def testFullRun(self):
    """Test a regular full run.

    This method is basically just a sanity check that it's trying to create the
    sysroot and install the toolchain by default.
    """
    target_sysroot = sysroot_lib.Sysroot('/build/board')
    create_mock = self.PatchObject(sysroot, 'Create',
                                   return_value=target_sysroot)
    install_toolchain_mock = self.PatchObject(sysroot, 'InstallToolchain')

    sysroot.SetupBoard(build_target_util.BuildTarget('board'))

    create_mock.assert_called_once()
    install_toolchain_mock.assert_called_once()

  def testRegenConfigs(self):
    """Test the regen configs install prevention."""
    target_sysroot = sysroot_lib.Sysroot('/build/board')
    create_mock = self.PatchObject(sysroot, 'Create',
                                   return_value=target_sysroot)
    install_toolchain_mock = self.PatchObject(sysroot, 'InstallToolchain')

    target = build_target_util.BuildTarget('board')
    configs = sysroot.SetupBoardRunConfig(regen_configs=True)

    sysroot.SetupBoard(target, run_configs=configs)

    # Should still try to create the sysroot, but should not try to install
    # the toolchain.
    create_mock.assert_called_once()
    install_toolchain_mock.assert_not_called()


class CreateTest(cros_test_lib.RunCommandTempDirTestCase):
  """Create function tests."""

  def setUp(self):
    # Avoid sudo password prompt for config writing.
    self.PatchObject(os, 'getuid', return_value=0)
    self.PatchObject(os, 'geteuid', return_value=0)

    # It has to be run inside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

    # A board we have a sysroot for already.
    self.board = 'board'
    self.sysroot_path = os.path.join(self.tempdir, 'build', self.board)
    self.build_target = build_target_util.BuildTarget(
        self.board, build_root=self.sysroot_path)
    # A board we don't have a sysroot for yet.
    self.unbuilt_board = 'board2'
    self.unbuilt_path = os.path.join(self.tempdir, 'build', self.unbuilt_board)
    self.unbuilt_target = build_target_util.BuildTarget(
        self.unbuilt_board, build_root=self.unbuilt_path)

    # Create the sysroot.
    osutils.SafeMakedirs(self.sysroot_path)

  def testUpdateChroot(self):
    """Test the update_chroot related handling."""
    # Prevent it from doing anything else for this test.
    self.PatchObject(sysroot, '_CreateSysrootSkeleton')
    self.PatchObject(sysroot, '_InstallConfigs')
    self.PatchObject(sysroot, '_InstallPortageConfigs')

    # Make sure we have a board we haven't setup to avoid triggering the
    # existing sysroot logic. That is entirely unrelated to the chroot update.
    target = self.unbuilt_target

    # Test no update case.
    config = sysroot.SetupBoardRunConfig(upgrade_chroot=False)
    get_args_patch = self.PatchObject(config, 'GetUpdateChrootArgs')

    sysroot.Create(target, config, None)

    # The update chroot args not being fetched is a
    # strong enough signal that the update wasn't run.
    get_args_patch.assert_not_called()

    # Test update case.
    script_loc = os.path.join(constants.CROSUTILS_DIR, 'update_chroot')
    config = sysroot.SetupBoardRunConfig(upgrade_chroot=True)

    sysroot.Create(target, config, None)

    self.assertCommandContains([script_loc])

  def testForce(self):
    """Test the force flag."""
    # Prevent it from doing anything else for this test.
    self.PatchObject(sysroot, '_CreateSysrootSkeleton')
    self.PatchObject(sysroot, '_InstallConfigs')
    self.PatchObject(sysroot, '_InstallPortageConfigs')

    delete_patch = self.PatchObject(sysroot_lib.Sysroot, 'Delete')

    config = sysroot.SetupBoardRunConfig(force=False)
    sysroot.Create(self.build_target, config, None)
    delete_patch.assert_not_called()

    config = sysroot.SetupBoardRunConfig(force=True)
    sysroot.Create(self.build_target, config, None)
    delete_patch.assert_called_once()


class InstallToolchainTest(cros_test_lib.MockTempDirTestCase):
  """Tests for InstallToolchain."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    # A board we have a sysroot for already.
    self.board = 'board'
    self.sysroot_path = os.path.join(self.tempdir, 'build', self.board)
    self.build_target = build_target_util.BuildTarget(
        self.board, build_root=self.sysroot_path)
    self.sysroot = sysroot_lib.Sysroot(self.sysroot_path)

    # A board we don't have a sysroot for yet.
    self.unbuilt_board = 'board2'
    self.unbuilt_path = os.path.join(self.tempdir, 'build', self.unbuilt_board)
    self.unbuilt_target = build_target_util.BuildTarget(
        self.unbuilt_board, build_root=self.unbuilt_path)
    self.unbuilt_sysroot = sysroot_lib.Sysroot(self.unbuilt_path)

    osutils.SafeMakedirs(self.sysroot_path)

  def testNoSysroot(self):
    """Test handling of no sysroot."""
    with self.assertRaises(ValueError):
      sysroot.InstallToolchain(self.unbuilt_target, self.unbuilt_sysroot,
                               sysroot.SetupBoardRunConfig())

  def testLocalBuild(self):
    """Test the local build logic."""
    update_patch = self.PatchObject(self.sysroot, 'UpdateToolchain')

    # Test False.
    config = sysroot.SetupBoardRunConfig(usepkg=False, local_build=False)
    sysroot.InstallToolchain(self.build_target, self.sysroot, config)
    update_patch.assert_called_with(self.board, local_init=False)

    # Test usepkg True.
    update_patch.reset_mock()
    config = sysroot.SetupBoardRunConfig(usepkg=True, local_build=False)
    sysroot.InstallToolchain(self.build_target, self.sysroot, config)
    update_patch.assert_called_with(self.board, local_init=True)

    # Test local_build True.
    update_patch.reset_mock()
    config = sysroot.SetupBoardRunConfig(usepkg=False, local_build=True)
    sysroot.InstallToolchain(self.build_target, self.sysroot, config)
    update_patch.assert_called_with(self.board, local_init=True)

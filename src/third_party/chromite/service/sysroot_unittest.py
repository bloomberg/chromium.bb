# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot service unittest."""

from __future__ import print_function

import os

import mock

from chromite.lib import build_target_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import portage_util
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

    sysroot.SetupBoard(build_target_lib.BuildTarget('board'))

    create_mock.assert_called_once()
    install_toolchain_mock.assert_called_once()

  def testRegenConfigs(self):
    """Test the regen configs install prevention."""
    target_sysroot = sysroot_lib.Sysroot('/build/board')
    create_mock = self.PatchObject(sysroot, 'Create',
                                   return_value=target_sysroot)
    install_toolchain_mock = self.PatchObject(sysroot, 'InstallToolchain')

    target = build_target_lib.BuildTarget('board')
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
    self.build_target = build_target_lib.BuildTarget(
        self.board, build_root=self.sysroot_path)
    # A board we don't have a sysroot for yet.
    self.unbuilt_board = 'board2'
    self.unbuilt_path = os.path.join(self.tempdir, 'build', self.unbuilt_board)
    self.unbuilt_target = build_target_lib.BuildTarget(
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


class CreateSimpleChromeSysrootTest(cros_test_lib.MockTempDirTestCase):
  """Tests for CreateSimpleChromeSysroot."""

  def setUp(self):
    self.run_mock = self.PatchObject(cros_build_lib, 'run', return_value=True)
    self.source_root = os.path.join(self.tempdir, 'source_root')
    osutils.SafeMakedirs(self.source_root)
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.source_root)

  def testCreateSimpleChromeSysroot(self):
    # A board for which we will create a simple chrome sysroot.
    target = 'board'
    use_flags = ['cros-debug', 'chrome_internal']

    # Call service, verify arguments passed to run.
    sysroot.CreateSimpleChromeSysroot(target, use_flags)
    self.run_mock.assert_called_with(
        ['cros_generate_sysroot', '--out-dir', mock.ANY, '--board', target,
         '--deps-only', '--package', 'chromeos-base/chromeos-chrome'],
        extra_env={'USE': 'cros-debug chrome_internal'},
        enter_chroot=True,
        cwd=self.source_root
    )


class GenerateArchiveTest(cros_test_lib.MockTempDirTestCase):
  """Tests for GenerateArchive."""

  def setUp(self):
    self.run_mock = self.PatchObject(cros_build_lib, 'run', return_value=True)
    self.chroot_path = os.path.join(self.tempdir, 'chroot_dir')

  def testCreateSimpleChromeSysroot(self):
    # A board for which we will create a simple chrome sysroot.
    target = 'board'
    pkg_list = ['virtual/target-fuzzers']

    # Call service, verify arguments passed to run.
    sysroot.GenerateArchive(self.chroot_path, target, pkg_list)
    self.run_mock.assert_called_with(
        ['cros_generate_sysroot', '--out-file', constants.TARGET_SYSROOT_TAR,
         '--out-dir', mock.ANY, '--board', target,
         '--package', 'virtual/target-fuzzers'],
        cwd=constants.SOURCE_ROOT
    )


class InstallToolchainTest(cros_test_lib.MockTempDirTestCase):
  """Tests for InstallToolchain."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    # A board we have a sysroot for already.
    self.board = 'board'
    self.sysroot_path = os.path.join(self.tempdir, 'build', self.board)
    self.build_target = build_target_lib.BuildTarget(
        self.board, build_root=self.sysroot_path)
    self.sysroot = sysroot_lib.Sysroot(self.sysroot_path)

    # A board we don't have a sysroot for yet.
    self.unbuilt_board = 'board2'
    self.unbuilt_path = os.path.join(self.tempdir, 'build', self.unbuilt_board)
    self.unbuilt_target = build_target_lib.BuildTarget(
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


class BuildPackagesRunConfigTest(cros_test_lib.TestCase):
  """Tests for the BuildPackagesRunConfig."""

  def AssertHasRequiredArgs(self, args):
    """Tests the default/required args for the builders."""
    self.assertIn('--accept_licenses', args)
    self.assertIn('@CHROMEOS', args)
    self.assertIn('--skip_chroot_upgrade', args)

  def testGetBuildPackagesDefaultArgs(self):
    """Test the build_packages args building for empty/false/0 values."""
    # Test False/None/0 values.
    instance = sysroot.BuildPackagesRunConfig(usepkg=False,
                                              install_debug_symbols=False,
                                              packages=None)

    args = instance.GetBuildPackagesArgs()
    self.AssertHasRequiredArgs(args)
    # Debug symbols not included.
    self.assertNotIn('--withdebugsymbols', args)
    # Source used.
    self.assertIn('--nousepkg', args)
    # Flag removed due to broken logic.  See crbug/1048419.
    self.assertNotIn('--reuse_pkgs_from_local_boards', args)

  def testGetBuildPackagesArgs(self):
    """Test the build_packages args building for non-empty values."""
    packages = ['cat/pkg', 'cat2/pkg2']
    instance = sysroot.BuildPackagesRunConfig(usepkg=True,
                                              install_debug_symbols=True,
                                              packages=packages)

    args = instance.GetBuildPackagesArgs()
    self.AssertHasRequiredArgs(args)
    # Local build not used.
    self.assertNotIn('--nousepkg', args)
    self.assertNotIn('--reuse_pkgs_from_local_boards', args)
    # Debug symbols included.
    self.assertIn('--withdebugsymbols', args)
    # Packages included.
    for package in packages:
      self.assertIn(package, args)


class BuildPackagesTest(cros_test_lib.RunCommandTestCase):
  """Test BuildPackages function."""

  def setUp(self):
    # Currently just used to keep the parallel emerge status file from being
    # created in the chroot. This probably isn't strictly necessary, but since
    # we can otherwise run this test without a chroot existing at all and
    # without touching the chroot folder, it's better to keep it out of there
    # all together.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

    self.board = 'board'
    self.target = build_target_lib.BuildTarget(self.board)
    self.sysroot_path = '/sysroot/path'
    self.sysroot = sysroot_lib.Sysroot(self.sysroot_path)

    self.build_packages = os.path.join(constants.CROSUTILS_DIR,
                                       'build_packages')
    self.base_command = [self.build_packages, '--board', self.board,
                         '--board_root', self.sysroot_path]

  def testSuccess(self):
    """Test successful run."""
    config = sysroot.BuildPackagesRunConfig()
    sysroot.BuildPackages(self.target, self.sysroot, config)

    # The rest of the command's args we test in BuildPackagesRunConfigTest,
    # so just make sure we're calling the right command and pass the args not
    # handled by the run config.
    self.assertCommandContains(self.base_command)

  def testPackageFailure(self):
    """Test package failure handling."""
    failed = ['cat/pkg', 'foo/bar']
    cpvs = [portage_util.SplitCPV(p, strict=False) for p in failed]
    self.PatchObject(portage_util, 'ParseDieHookStatusFile',
                     return_value=cpvs)

    config = sysroot.BuildPackagesRunConfig()
    command = self.base_command + config.GetBuildPackagesArgs()

    result = cros_build_lib.CommandResult(cmd=command, returncode=1)
    error = cros_build_lib.RunCommandError('Error', result)

    self.rc.AddCmdResult(command, side_effect=error)

    try:
      sysroot.BuildPackages(self.target, self.sysroot, config)
    except sysroot_lib.PackageInstallError as e:
      self.assertEqual(cpvs, e.failed_packages)
      self.assertEqual(result, e.result)
    except Exception as e:
      self.fail('Unexpected exception type: %s' % type(e))
    else:
      self.fail('Expected an exception to be thrown.')

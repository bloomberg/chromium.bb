# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for workspace stages."""

from __future__ import print_function

import os

import mock

from chromite.cbuildbot.builders import workspace_builders_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import workspace_stages
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import portage_util

# pylint: disable=too-many-ancestors
# pylint: disable=protected-access


class WorkspaceStageBase(
    generic_stages_unittest.RunCommandAbstractStageTestCase):
  """Base class for test suites covering workspace stages."""

  # Default version for tests.
  OLD_VERSION = '1.2.3'

  # Version newer than all "limits" in workspace_stages.
  MODERN_VERSION = '11900.0.0'

  def setUp(self):
    self.workspace = os.path.join(self.tempdir, 'workspace')
    # Make it a 'repo' for chroot path conversions.
    osutils.SafeMakedirs(os.path.join(self.workspace, '.repo'))

    self.from_repo_mock = self.PatchObject(
        manifest_version.VersionInfo, 'from_repo')
    self.SetWorkspaceVersion(self.OLD_VERSION)

    self.manifest_versions = os.path.join(
        self.build_root, 'manifest-versions')
    self.manifest_versions_int = os.path.join(
        self.build_root, 'manifest-versions-internal')

    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)

  def SetWorkspaceVersion(self, version, chrome_branch='1'):
    """Change the "version" of the workspace."""
    self.from_repo_mock.return_value = manifest_version.VersionInfo(
        version, chrome_branch=chrome_branch)

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.

    Note: Must be implemented in subclasses.
    """
    raise NotImplementedError(self, 'ConstructStage: Implement in your test')


class WorkspaceStageBaseTest(WorkspaceStageBase):
  """Test the WorkspaceStageBase."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceStageBase(
        self._run, self.buildstore, build_root=self.workspace)

  def testBuildRoots(self):
    """Tests that various properties are correctly set."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    stage = self.ConstructStage()

    # Verify buildroot directories.
    self.assertEqual(self.build_root, stage._orig_root)
    self.assertEqual(self.workspace, stage._build_root)

    # Verify repo creation.
    repo = stage.GetWorkspaceRepo()
    self.assertEqual(repo.directory, self.workspace)

    # Verify manifest-versions directories.
    self.assertEqual(
        self.manifest_versions,
        stage.ext_manifest_versions_path)
    self.assertEqual(
        self.manifest_versions_int,
        stage.int_manifest_versions_path)

  def testVersionInfo(self):
    """Tests GetWorkspaceVersionInfo."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    stage = self.ConstructStage()

    stage.GetWorkspaceVersionInfo()

    self.assertEqual(self.from_repo_mock.call_args_list, [
        mock.call(self.workspace),
    ])

  def testAfterLimit(self):
    """Tests AfterLimit."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    stage = self.ConstructStage()

    # 1.2.3
    self.SetWorkspaceVersion(self.OLD_VERSION)

    LIMITS_BEFORE_OLD = ('1.0.0', '1.0.5', '1.2.2')
    LIMITS_AFTER_OLD = ('1.2.3', '1.2.4', '2.0.0')

    # The workspace is after these limits.
    for l in LIMITS_BEFORE_OLD:
      self.assertTrue(stage.AfterLimit(l))

    # The workspace is before these limits.
    for l in LIMITS_AFTER_OLD:
      self.assertFalse(stage.AfterLimit(l))

    # 11000.0.0
    self.SetWorkspaceVersion(self.MODERN_VERSION)

    # The workspace is after ALL of these limits.
    for l in LIMITS_BEFORE_OLD + LIMITS_AFTER_OLD:
      self.assertTrue(stage.AfterLimit(l))


class SyncStageTest(WorkspaceStageBase):
  """Test the SyncStage."""

  def ConstructStage(self, **kwargs):
    return workspace_stages.SyncStage(
        self._run, self.buildstore, **kwargs)

  def testDefaults(self):
    """Tests sync command used by default."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage(build_root='/root')

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled([
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', '/root',
        '--manifest-versions-int', self.manifest_versions_int,
        '--manifest-versions-ext', self.manifest_versions,
    ])

  def testBranch(self):
    """Tests sync command used for branch."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage(build_root='/root', branch='branch')

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled([
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', '/root',
        '--manifest-versions-int', self.manifest_versions_int,
        '--manifest-versions-ext', self.manifest_versions,
        '--branch', 'branch',
    ])

  def testVersion(self):
    """Tests sync command used for version."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage(build_root='/root', version='version')

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled([
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', '/root',
        '--manifest-versions-int', self.manifest_versions_int,
        '--manifest-versions-ext', self.manifest_versions,
        '--version', 'version',
    ])

  def testPatches(self):
    """Tests sync command used with patches."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    patchA = mock.Mock()
    patchA.url = 'urlA'
    patchA.gerrit_number_str = '1'

    patchB = mock.Mock()
    patchB.url = 'urlB'
    patchB.gerrit_number_str = '2'

    self.RunStage(build_root='/root', patch_pool=[patchA, patchB])

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled([
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', '/root',
        '--manifest-versions-int', self.manifest_versions_int,
        '--manifest-versions-ext', self.manifest_versions,
        '--gerrit-patches', '1',
        '--gerrit-patches', '2',
    ])

  def testMax(self):
    """Tests sync command with as many options as possible."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    patchA = mock.Mock()
    patchA.url = 'urlA'
    patchA.gerrit_number_str = '1'

    patchB = mock.Mock()
    patchB.url = 'urlB'
    patchB.gerrit_number_str = '2'

    self.RunStage(build_root='/root',
                  branch='branch',
                  patch_pool=[patchA, patchB],
                  copy_repo='/source')

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled([
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', '/root',
        '--manifest-versions-int', self.manifest_versions_int,
        '--manifest-versions-ext', self.manifest_versions,
        '--branch', 'branch',
        '--gerrit-patches', '1',
        '--gerrit-patches', '2',
        '--copy-repo', '/source',
    ])


class WorkspaceSyncStageTest(WorkspaceStageBase):
  """Test the WorkspaceSyncStage."""

  def setUp(self):
    self.sync_stage_mock = self.PatchObject(
        workspace_stages, 'SyncStage')

  def ConstructStage(self):
    return workspace_stages.WorkspaceSyncStage(
        self._run, self.buildstore, build_root=self.workspace)

  def SyncCallToPathNumbers(self, mock_call):
    """Extract the patch_pool from a mock call, and convert to gerrit int."""
    return [p.gerrit_number_str for p in mock_call[1]['patch_pool']]

  def testBasic(self):
    """Test invoking child syncs in standard case."""
    self._Prepare(
        'buildspec',
        site_config=workspace_builders_unittest.CreateMockSiteConfig())

    self.RunStage()

    self.assertEqual(self.sync_stage_mock.call_args_list, [
        mock.call(self._run, self.buildstore,
                  patch_pool=mock.ANY,
                  suffix=' [Infra]',
                  external=True,
                  branch='master',
                  build_root=self.build_root),

        mock.call(self._run, self.buildstore,
                  patch_pool=mock.ANY,
                  suffix=' [test-branch]',
                  external=False,
                  branch='test-branch',
                  version=None,
                  build_root=self.workspace,
                  copy_repo=self.build_root),
    ])

    self.assertEqual(
        self.SyncCallToPathNumbers(self.sync_stage_mock.call_args_list[0]),
        [])

    self.assertEqual(
        self.SyncCallToPathNumbers(self.sync_stage_mock.call_args_list[1]),
        [])

  # TODO(dgarrett): Enable. Failing in _Prepare, and I don't understand why.
  def notestPatches(self):
    """Test invoking child syncs with patches to apply."""

    self._Prepare(
        cmd_args=[
            '-r', self.build_root, '--buildbot', '--noprebuilts',
            '--branch', self.TARGET_MANIFEST_BRANCH,
            '-g', '1', '-g', '2',
            'buildspec',
        ],
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
    )

    self.RunStage()

    self.assertEqual(self.sync_stage_mock.call_args_list, [
        mock.call(self._run, self.buildstore,
                  patch_pool=mock.ANY,
                  suffix=' [Infra]',
                  external=True,
                  branch='master',
                  build_root=self.build_root),

        mock.call(self._run, self.buildstore,
                  patch_pool=mock.ANY,
                  suffix=' [test-branch]',
                  external=True,
                  branch='test-branch',
                  version=None,
                  build_root=self.workspace,
                  copy_repo=self.build_root),
    ])

    self.assertEqual(
        self.SyncCallToPathNumbers(self.sync_stage_mock.call_args_list[0]),
        [])

    self.assertEqual(
        self.SyncCallToPathNumbers(self.sync_stage_mock.call_args_list[1]),
        [])


class WorkspaceSyncChromeStageTest(WorkspaceStageBase):
  """Test the WorkspaceSyncChromeStage."""

  def setUp(self):
    # Fake portage_util.CPV with the only field we need set.
    fake_cpv = mock.Mock()
    fake_cpv.version_no_rev = '0.0.1'
    self.mock_best_visible = self.PatchObject(
        portage_util, 'PortageqBestVisible',
        return_value=fake_cpv)

  def ConstructStage(self):
    return workspace_stages.WorkspaceSyncChromeStage(
        self._run, self.buildstore, build_root=self.workspace)

  def testNormal(self):
    """Test SyncChrome with normal usage."""
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--chrome_root', '/chrome'])

    self.RunStage()

    self.assertEqual(
        self.mock_best_visible.call_args_list,
        [
            mock.call('chromeos-base/chromeos-chrome', cwd=self.workspace)
        ])

    self.assertEqual(
        self.rc.call_args_list,
        [
            mock.call([os.path.join(self.build_root,
                                    'chromite/bin/sync_chrome'),
                       '--reset', '--ignore_locks',
                       '--gclient', os.path.join(
                           self.workspace,
                           'chromium/tools/depot_tools/gclient'),
                       '--tag', '0.0.1',
                       '--git_cache_dir', mock.ANY,
                       '--internal',
                       '/chrome'],
                      cwd=self.workspace),
        ])


# TODO(dgarret): Test WorkspaceUprevAndPublishStage
# TODO(dgarret): Test WorkspacePublishBuildspecStage
# TODO(dgarret): Test WorkspaceScheduleChildrenStage

class WorkspaceInitSDKStageTest(WorkspaceStageBase):
  """Test the WorkspaceInitSDKStage."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceInitSDKStage(
        self._run, self.buildstore, build_root=self.workspace)

  def testInitSDK(self):
    """Test InitSDK old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            os.path.join(self.workspace, 'chromite', 'bin', 'cros_sdk'),
            '--create',
            '--cache-dir', '/cache',
        ],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testInitSDKWithChrome(self):
    """Test InitSDK old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            os.path.join(self.workspace, 'chromite', 'bin', 'cros_sdk'),
            '--create',
            '--cache-dir', '/cache',
            '--chrome_root', '/chrome',
        ],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
            'CHROME_ORIGIN': 'LOCAL_SOURCE',
        },
        cwd=self.workspace,
    )


class WorkspaceUpdateSDKStageTest(WorkspaceStageBase):
  """Test the WorkspaceUpdateSDKStage."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceUpdateSDKStage(
        self._run, self.buildstore, build_root=self.workspace)

  def testUpdateSDK(self):
    """Test UpdateSDK old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        ['./update_chroot',],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug -separatedebug splitdebug',
        },
        cwd=self.workspace,
    )

  def testUpdateSDKWithChrome(self):
    """Test UpdateSDK old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        ['./update_chroot',],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'CHROME_ORIGIN': 'LOCAL_SOURCE',
            'FEATURES': 'separatedebug -separatedebug splitdebug',
            'USE': '-cros-debug chrome_internal chromeless_tty',
        },
        cwd=self.workspace,
    )


class WorkspaceSetupBoardStageTest(WorkspaceStageBase):
  """Test the WorkspaceSetupBoardStage."""

  def setUp(self):
    # Prevent the setup_board tempdir path from being translated because it
    # ends up raising an error when that path can't be found in the chroot.
    self.PatchObject(path_util, 'ToChrootPath', side_effect=lambda x: x)
    self.setup_board = os.path.join(self.workspace,
                                    constants.CHROMITE_BIN_SUBDIR,
                                    'setup_board')

  def ConstructStage(self):
    return workspace_stages.WorkspaceSetupBoardStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testSetupBoard(self):
    """Test setup_board old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            './setup_board',
            '--board=board',
            '--accept_licenses=@CHROMEOS',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testSetupBoardModern(self):
    """Test setup_board modern workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.SetWorkspaceVersion(self.MODERN_VERSION)
    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            self.setup_board,
            '--board=board',
            '--accept-licenses=@CHROMEOS',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testSetupBoardWithChrome(self):
    """Test setup_board old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'])

    self.SetWorkspaceVersion(self.OLD_VERSION)
    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            './setup_board',
            '--board=board',
            '--accept_licenses=@CHROMEOS',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
            'CHROME_ORIGIN': 'LOCAL_SOURCE',
        },
        cwd=self.workspace,
    )

  def testSetupBoardWithChromeModern(self):
    """Test setup_board modern workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'])

    self.SetWorkspaceVersion(self.MODERN_VERSION)
    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            self.setup_board,
            '--board=board',
            '--accept-licenses=@CHROMEOS',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'],
        extra_env={
            'USE': '-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
            'CHROME_ORIGIN': 'LOCAL_SOURCE',
        },
        cwd=self.workspace,
    )


class WorkspaceBuildPackagesStageTest(WorkspaceStageBase):
  """Test the WorkspaceBuildPackagesStage."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceBuildPackagesStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testFirmwareBuildPackagesOld(self):
    """Test building firmware for an old workspace version."""
    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            './build_packages',
            '--board=board',
            '--accept_licenses=@CHROMEOS',
            '--skip_chroot_upgrade',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
            'virtual/chromeos-firmware',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': u'-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testFirmwareBuildPackagesModern(self):
    """Test building firmware for a modern workspace version."""
    self.SetWorkspaceVersion(self.MODERN_VERSION)

    self._Prepare(
        'test-firmwarebranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            './build_packages',
            '--board=board',
            '--accept_licenses=@CHROMEOS',
            '--skip_chroot_upgrade',
            '--withdebugsymbols',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
            'virtual/chromeos-firmware',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': u'-cros-debug chrome_internal chromeless_tty',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testFactoryBuildPackages(self):
    """Test building factory for an old workspace version."""
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)
    self.rc.assertCommandCalled(
        [
            './build_packages',
            '--board=board',
            '--accept_licenses=@CHROMEOS',
            '--skip_chroot_upgrade',
            '--nousepkg',
            '--reuse_pkgs_from_local_boards',
            'virtual/target-os',
            'virtual/target-os-dev',
            'virtual/target-os-test',
            'virtual/target-os-factory',
            'virtual/target-os-factory-shim',
            'chromeos-base/autotest-all',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache', '--chrome_root', '/chrome'],
        extra_env={
            'USE': u'-cros-debug chrome_internal',
            'FEATURES': 'separatedebug',
            'CHROME_ORIGIN': 'LOCAL_SOURCE',
        },
        cwd=self.workspace,
    )


class WorkspaceUnitTestStageTest(WorkspaceStageBase):
  """Test the workspace_stages classes."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceUnitTestStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testFactoryOld(self):
    self.SetWorkspaceVersion(self.OLD_VERSION)

    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_args_list, [])

  def testFactoryNew(self):
    self.SetWorkspaceVersion(self.MODERN_VERSION)

    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(
        self.rc.call_args_list,
        [
            mock.call(
                ['/mnt/host/source/chromite/bin/cros_run_unit_tests',
                 '--board=board'],
                enter_chroot=True,
                chroot_args=['--cache-dir', '/cache'],
                extra_env={'USE': '-cros-debug chrome_internal'},
                cwd=self.workspace,
            ),
        ]
    )


class WorkspaceBuildImageStageTest(WorkspaceStageBase):
  """Test the workspace_stages classes."""

  def ConstructStage(self):
    return workspace_stages.WorkspaceBuildImageStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testFactoryOld(self):
    self.SetWorkspaceVersion(self.OLD_VERSION)
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)

    # Ensure no --builderpath
    self.rc.assertCommandCalled(
        [
            './build_image',
            '--board', 'board',
            '--replace',
            '--version', 'R1-1.2.3',
            'test',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )

  def testFactoryModern(self):
    self.SetWorkspaceVersion(self.MODERN_VERSION)
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.assertEqual(self.rc.call_count, 1)

    # Ensure has --builderpath
    self.rc.assertCommandCalled(
        [
            './build_image',
            '--board', 'board',
            '--replace',
            '--version', 'R1-%s' % self.MODERN_VERSION,
            '--builder_path', 'test-factorybranch/R1-%s' % self.MODERN_VERSION,
            'test',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace,
    )


class WorkspaceDebugSymbolsStageTest(WorkspaceStageBase):
  """Test the workspace_stages classes."""

  def setUp(self):
    self.tarball_mock = self.PatchObject(
        commands, 'GenerateDebugTarball',
        side_effect=('/debug.tgz', '/breakpad.tgz'))

    self.archive_mock = self.PatchObject(
        generic_stages.ArchivingStageMixin, 'UploadArtifact')

  def ConstructStage(self):
    # Version for the infra branch.
    self._run.attrs.version_info = manifest_version.VersionInfo(
        '10.0.0', chrome_branch='10')
    self._run.attrs.release_tag = 'infra-tag'

    return workspace_stages.WorkspaceDebugSymbolsStage(
        self._run, self.buildstore, build_root=self.workspace, board='board')

  def testFactory(self):
    """Test with a generic factory build config."""
    self._Prepare(
        'test-factorybranch',
        site_config=workspace_builders_unittest.CreateMockSiteConfig(),
        extra_cmd_args=['--cache-dir', '/cache'])

    self.RunStage()

    self.rc.assertCommandCalled(
        [
            '/mnt/host/source/chromite/bin/cros_generate_breakpad_symbols',
            '--board=board',
            '--jobs', mock.ANY,
            '--exclude-dir=firmware',
        ],
        enter_chroot=True,
        chroot_args=['--cache-dir', '/cache'],
        extra_env={
            'USE': '-cros-debug chrome_internal',
            'FEATURES': 'separatedebug',
        },
        cwd=self.workspace)

    self.assertEqual(
        self.tarball_mock.call_args_list,
        [
            mock.call(buildroot=self.workspace,
                      board='board',
                      archive_path=os.path.join(
                          self.build_root,
                          'buildbot_archive/test-factorybranch/R10-infra-tag'),
                      gdb_symbols=True,
                      archive_name='debug.tgz',
                      chroot_compression=False),
            mock.call(buildroot=self.workspace,
                      board='board',
                      archive_path=os.path.join(
                          self.build_root,
                          'buildbot_archive/test-factorybranch/R10-infra-tag'),
                      gdb_symbols=False,
                      archive_name='debug_breakpad.tar.xz',
                      chroot_compression=False),
        ],
    )

    self.assertEqual(
        self.archive_mock.call_args_list,
        [
            mock.call('/debug.tgz', archive=False),
            mock.call('/breakpad.tgz', archive=False),
        ],
    )

# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The test service unit tests."""

from __future__ import print_function

import contextlib
import os
import shutil

import mock

from chromite.api.gen.chromiumos import common_pb2
from chromite.cbuildbot import commands
from chromite.cbuildbot import goma_util
from chromite.lib import build_target_util
from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import image_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import test


class BuildTargetUnitTestResultTest(cros_test_lib.TestCase):
  """BuildTargetUnitTestResult tests."""

  def testSuccess(self):
    """Test success case."""
    result = test.BuildTargetUnitTestResult(0, None)
    self.assertTrue(result.success)

  def testPackageFailure(self):
    """Test packages failed."""
    # Supposed to be CPVs, but not actually necessary at the moment.
    packages = ['a', 'b']
    # Should have a non-zero return code when packages fail.
    result = test.BuildTargetUnitTestResult(1, packages)
    self.assertFalse(result.success)
    # Make sure failed packages alone are enough.
    result = test.BuildTargetUnitTestResult(0, packages)
    self.assertFalse(result.success)

  def testScriptFailure(self):
    """Test non-package failure."""
    # Should have a non-zero return code when packages fail.
    result = test.BuildTargetUnitTestResult(1, None)
    self.assertFalse(result.success)


class BuildTargetUnitTestTest(cros_test_lib.RunCommandTempDirTestCase):
  """BuildTargetUnitTest tests."""

  def setUp(self):
    self.board = 'board'
    self.build_target = build_target_util.BuildTarget(self.board)
    self.chroot = chroot_lib.Chroot(path=self.tempdir)
    # Make the chroot's tmp directory, used for the parallel emerge status file.
    tempdir = os.path.join(self.tempdir, 'tmp')
    osutils.SafeMakedirs(tempdir)

  def testSuccess(self):
    """Test simple success case."""
    result = test.BuildTargetUnitTest(self.build_target, self.chroot)

    self.assertCommandContains(['cros_run_unit_tests', '--board', self.board])
    self.assertTrue(result.success)

  def testBlacklist(self):
    """Test the blacklist argument."""
    blacklist = ['foo/bar', 'cat/pkg']
    test.BuildTargetUnitTest(self.build_target, self.chroot,
                             blacklist=blacklist)
    self.assertCommandContains(['--blacklist_packages', 'foo/bar cat/pkg'])

  def testFailure(self):
    """Test non-zero return code and failed package handling."""
    packages = ['foo/bar', 'cat/pkg']
    cpvs = [portage_util.SplitCPV(p, strict=False) for p in packages]
    self.PatchObject(portage_util, 'ParseDieHookStatusFile',
                     return_value=cpvs)
    expected_rc = 1
    self.rc.SetDefaultCmdResult(returncode=expected_rc)

    result = test.BuildTargetUnitTest(self.build_target, self.chroot)

    self.assertFalse(result.success)
    self.assertEqual(expected_rc, result.return_code)
    self.assertCountEqual(cpvs, result.failed_cpvs)


class BuildTargetUnitTestTarballTest(cros_test_lib.MockTestCase):
  """BuildTargetUnitTestTarball tests."""

  def setUp(self):
    self.chroot = chroot_lib.Chroot(path='/chroot/path')
    self.sysroot = sysroot_lib.Sysroot('/chroot/path/sysroot/path')
    self.result_path = '/result/path'

  def testSuccess(self):
    """Test success handling."""
    result = cros_build_lib.CommandResult(returncode=0)
    self.PatchObject(cros_build_lib, 'CreateTarball', return_value=result)

    path = test.BuildTargetUnitTestTarball(self.chroot, self.sysroot,
                                           self.result_path)

    self.assertStartsWith(path, self.result_path)

  def testFailure(self):
    """Test failure creating tarball."""
    result = cros_build_lib.CommandResult(returncode=1)
    self.PatchObject(cros_build_lib, 'CreateTarball', return_value=result)

    path = test.BuildTargetUnitTestTarball(self.chroot, self.sysroot,
                                           self.result_path)

    self.assertIsNone(path)


class DebugInfoTestTest(cros_test_lib.RunCommandTestCase):
  """DebugInfoTest tests."""

  def testSuccess(self):
    """Test command success."""
    self.assertTrue(test.DebugInfoTest('/sysroot/path'))
    self.assertCommandContains(['debug_info_test',
                                '/sysroot/path/usr/lib/debug'])

  def testFailure(self):
    """Test command failure."""
    self.rc.SetDefaultCmdResult(returncode=1)
    self.assertFalse(test.DebugInfoTest('/sysroot/path'))


class MoblabVmTestCase(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for the SetupBoardRunConfig class."""

  def MockDirectory(self, path):
    """Create an empty directory.

    Args:
      path (str): Relative path for the directory.

    Returns:
      str: Path to the directory.
    """
    path = os.path.join(self.tempdir, path)
    osutils.SafeMakedirs(path)
    return path

  def setUp(self):
    self.builder = 'moblab-generic-vm/R12-3.4.5-67-890'
    self.image_dir = self.MockDirectory('files/image')
    self.payload_dir = self.MockDirectory('files/payload')
    self.results_dir = self.MockDirectory('results')
    self.vms = moblab_vm.MoblabVm(self.tempdir)
    self.chroot = chroot_lib.Chroot(path=self.tempdir)


class CreateMoblabVmTest(MoblabVmTestCase):
  """Unit tests for CreateMoblabVm."""

  def setUp(self):
    self.mock_vm_create = self.PatchObject(moblab_vm.MoblabVm, 'Create')

  def testBasic(self):
    vms = test.CreateMoblabVm(self.tempdir, self.chroot.path, self.image_dir)
    self.assertEqual(vms.workspace, self.tempdir)
    self.assertEqual(vms.chroot, self.chroot.path)
    self.assertEqual(
        self.mock_vm_create.call_args_list,
        [mock.call(self.image_dir, dut_image_dir=self.image_dir,
                   create_vm_images=False)])


class PrepareMoblabVmImageCacheTest(MoblabVmTestCase):
  """Unit tests for PrepareMoblabVmImageCache."""

  def setUp(self):
    @contextlib.contextmanager
    def MountedMoblabDiskContextMock(*_args, **_kwargs):
      yield self.tempdir

    self.PatchObject(moblab_vm.MoblabVm, 'MountedMoblabDiskContext',
                     MountedMoblabDiskContextMock)

    self.payload_file_name = 'payload.bin'
    self.payload_file = os.path.join(self.payload_dir, self.payload_file_name)
    self.payload_file_content = 'A Lannister always pays his debts.'
    osutils.WriteFile(os.path.join(self.payload_dir, self.payload_file_name),
                      self.payload_file_content)

  def testBasic(self):
    """PrepareMoblabVmImageCache loads all payloads into the vm."""
    image_cache_dir = test.PrepareMoblabVmImageCache(self.vms, self.builder,
                                                     [self.payload_dir])
    expected_cache_dir = 'static/prefetched/moblab-generic-vm/R12-3.4.5-67-890'
    self.assertEqual(image_cache_dir,
                     os.path.join('/mnt/moblab/', expected_cache_dir))

    copied_payload_file = os.path.join(self.tempdir, expected_cache_dir,
                                       self.payload_file_name)
    self.assertExists(copied_payload_file)
    self.assertEqual(osutils.ReadFile(copied_payload_file),
                     self.payload_file_content)


class RunMoblabVmTestTest(MoblabVmTestCase):
  """Unit tests for RunMoblabVmTestTest."""

  def setUp(self):
    self.image_cache_dir = '/mnt/moblab/whatever'
    self.PatchObject(moblab_vm.MoblabVm, 'Start')
    self.PatchObject(moblab_vm.MoblabVm, 'Stop')

  def testBasic(self):
    """RunMoblabVmTest calls test_that with correct args."""
    test.RunMoblabVmTest(self.chroot, self.vms, self.builder,
                         self.image_cache_dir, self.results_dir)
    self.assertCommandContains([
        'test_that', '--no-quickmerge',
        '--results_dir', self.results_dir,
        '-b', 'moblab-generic-vm',
        'moblab_DummyServerNoSspSuite',
        '--args',
        'services_init_timeout_m=10 '
        'target_build="%s" '
        'test_timeout_hint_m=90 '
        'clear_devserver_cache=False '
        'image_storage_server="%s"' % (self.builder,
                                       self.image_cache_dir + '/'),
    ], enter_chroot=True, chroot_args=self.chroot.get_enter_args())


class SimpleChromeWorkflowTestTest(cros_test_lib.MockTempDirTestCase):
  """Unit tests for SimpleChromeWorkflowTest."""

  def setUp(self):
    self.chrome_root = '/path/to/chrome/root'
    self.sysroot_path = '/chroot/path/sysroot/path'
    self.build_target = 'board'

    self.goma_mock = self.PatchObject(goma_util, 'Goma')

    self.chrome_sdk_run_mock = self.PatchObject(commands.ChromeSDK, 'Run')

    # SimpleChromeTest workflow creates directories based on objects that are
    # mocked for this test, so patch osutils.WriteFile
    self.write_mock = self.PatchObject(osutils, 'WriteFile')

    self.PatchObject(cros_build_lib, 'CmdToStr', return_value='CmdToStr value')
    self.PatchObject(shutil, 'copy2')

  def testSimpleChromeWorkflowTest(self):
    goma_test_dir = os.path.join(self.tempdir, 'goma_test_dir')
    goma_test_json_string = os.path.join(self.tempdir, 'goma_json_string.txt')
    chromeos_goma_dir = os.path.join(self.tempdir, 'chromeos_goma_dir')
    goma_config = common_pb2.GomaConfig(goma_dir=goma_test_dir,
                                        goma_client_json=goma_test_json_string)
    osutils.SafeMakedirs(goma_test_dir)
    osutils.SafeMakedirs(chromeos_goma_dir)
    osutils.Touch(goma_test_json_string)
    goma = goma_util.Goma(
        goma_config.goma_dir,
        goma_config.goma_client_json,
        stage_name='BuildApiTestSimpleChrome',
        chromeos_goma_dir=chromeos_goma_dir)

    mock_goma_log_dir = os.path.join(self.tempdir, 'goma_log_dir')
    osutils.SafeMakedirs(mock_goma_log_dir)
    goma.goma_log_dir = mock_goma_log_dir

    # For this test, we avoid running test._VerifySDKEnvironment because use of
    # other mocks prevent creating the SDK dir that _VerifySDKEnvironment checks
    # for
    self.PatchObject(test, '_VerifySDKEnvironment')

    self.PatchObject(os.path, 'exists', return_value=True)


    ninja_cmd = self.PatchObject(commands.ChromeSDK, 'GetNinjaCommand',
                                 return_value='ninja command')

    test.SimpleChromeWorkflowTest(self.sysroot_path, self.build_target,
                                  self.chrome_root, goma)
    # Verify ninja_cmd calls.
    ninja_calls = [mock.call(), mock.call(debug=False)]
    ninja_cmd.assert_has_calls(ninja_calls)

    # Verify calls with args to chrome_sdk_run made by service/test.py.
    gn_dir = os.path.join(self.chrome_root, 'buildtools/linux64/gn')
    board_out_dir = os.path.join(self.chrome_root, 'out_board/Release')

    self.chrome_sdk_run_mock.assert_any_call(['gclient', 'runhooks'])
    self.chrome_sdk_run_mock.assert_any_call(['true'])
    self.chrome_sdk_run_mock.assert_any_call(
        ['bash', '-c', ('%s gen "%s" --args="$GN_ARGS"'
                        % (gn_dir, board_out_dir))])
    self.chrome_sdk_run_mock.assert_any_call(
        ['env', '--null'], run_args=mock.ANY)
    self.chrome_sdk_run_mock.assert_any_call('ninja command', run_args=mock.ANY)

    # Create expected paths from constants so that the tests work inside or
    # outside the SDK.
    deploy_chrome_path = os.path.join(constants.SOURCE_ROOT,
                                      constants.CHROMITE_BIN_SUBDIR,
                                      'deploy_chrome')
    image_dir_symlink = image_lib.GetLatestImageLink(self.build_target)
    image_path = os.path.join(image_dir_symlink, constants.VM_IMAGE_BIN)

    self.chrome_sdk_run_mock.assert_any_call(
        [deploy_chrome_path, '--build-dir', board_out_dir, '--staging-only',
         '--staging-dir', mock.ANY])
    self.chrome_sdk_run_mock.assert_any_call(
        ['cros_run_test', '--copy-on-write', '--deploy', '--board=board',
         ('--image-path=%s' % (image_path)),
         '--build-dir=out_board/Release'])

    # Verify goma mock was started and stopped.
    self.goma_mock.Start.assert_called_once()
    self.goma_mock.Stop.assert_called_once()


class ValidateMoblabVmTestTest(MoblabVmTestCase):
  """Unit tests for ValidateMoblabVmTest."""

  def setUp(self):
    self.logs_dir = os.path.join(self.results_dir, 'debug')
    osutils.SafeMakedirs(self.logs_dir)
    self.logs_file = os.path.join(self.logs_dir, 'test_that.INFO')

  def testValidateMoblabVmTestSuccess(self):
    """ValidateMoblabVmTest does not die when tests succeeded."""
    osutils.WriteFile(self.logs_file, 'dummy_PassServer [PASSED]')
    test.ValidateMoblabVmTest(self.results_dir)

  def testValidateMoblabVmTestNoLogs(self):
    """ValidateMoblabVmTest dies when test_that logs not present."""
    self.assertRaises(failures_lib.TestFailure,
                      test.ValidateMoblabVmTest, self.results_dir)

  def testValidateMoblabVmTestFailure(self):
    """ValidateMoblabVmTest dies when tests failed."""
    osutils.WriteFile(self.logs_file, 'dummy_PassServer [FAILED]')
    self.assertRaises(failures_lib.TestFailure,
                      test.ValidateMoblabVmTest, self.results_dir)

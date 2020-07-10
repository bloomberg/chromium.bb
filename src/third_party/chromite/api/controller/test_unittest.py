# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The test controller tests."""

from __future__ import print_function

import os

import contextlib
import mock

from chromite.api import api_config
from chromite.api import controller
from chromite.api.controller import test as test_controller
from chromite.api.gen.chromiumos import common_pb2
from chromite.api.gen.chromite.api import test_pb2
from chromite.lib import chroot_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.scripts import cros_set_lsb_release
from chromite.service import test as test_service
from chromite.utils import key_value_store


class DebugInfoTestTest(cros_test_lib.MockTempDirTestCase,
                        api_config.ApiConfigMixin):
  """Tests for the DebugInfoTest function."""

  def setUp(self):
    self.board = 'board'
    self.chroot_path = os.path.join(self.tempdir, 'chroot')
    self.sysroot_path = '/build/board'
    self.full_sysroot_path = os.path.join(self.chroot_path,
                                          self.sysroot_path.lstrip(os.sep))
    osutils.SafeMakedirs(self.full_sysroot_path)

  def _GetInput(self, sysroot_path=None, build_target=None):
    """Helper to build an input message instance."""
    proto = test_pb2.DebugInfoTestRequest()
    if sysroot_path:
      proto.sysroot.path = sysroot_path
    if build_target:
      proto.sysroot.build_target.name = build_target
    return proto

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return test_pb2.DebugInfoTestResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(test_service, 'DebugInfoTest')
    input_msg = self._GetInput(sysroot_path=self.full_sysroot_path)
    test_controller.DebugInfoTest(input_msg, self._GetOutput(),
                                  self.validate_only_config)
    patch.assert_not_called()

  def testNoBuildTargetNoSysrootFails(self):
    """Test missing build target name and sysroot path fails."""
    input_msg = self._GetInput()
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.DebugInfoTest(input_msg, output_msg, self.api_config)

  def testDebugInfoTest(self):
    """Call DebugInfoTest with valid sysroot_path."""
    request = self._GetInput(sysroot_path=self.full_sysroot_path)

    test_controller.DebugInfoTest(request, self._GetOutput(), self.api_config)


class BuildTargetUnitTestTest(cros_test_lib.MockTempDirTestCase,
                              api_config.ApiConfigMixin):
  """Tests for the UnitTest function."""

  def _GetInput(self, board=None, result_path=None, chroot_path=None,
                cache_dir=None, empty_sysroot=None, blacklist=None):
    """Helper to build an input message instance."""
    formatted_blacklist = []
    for pkg in blacklist or []:
      formatted_blacklist.append({'category': pkg.category,
                                  'package_name': pkg.package})

    return test_pb2.BuildTargetUnitTestRequest(
        build_target={'name': board}, result_path=result_path,
        chroot={'path': chroot_path, 'cache_dir': cache_dir},
        flags={'empty_sysroot': empty_sysroot},
        package_blacklist=formatted_blacklist,
    )

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return test_pb2.BuildTargetUnitTestResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(test_service, 'BuildTargetUnitTest')

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    test_controller.BuildTargetUnitTest(input_msg, self._GetOutput(),
                                        self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(test_service, 'BuildTargetUnitTest')

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    response = self._GetOutput()
    test_controller.BuildTargetUnitTest(input_msg, response,
                                        self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(response.tarball_path,
                     os.path.join(input_msg.result_path, 'unit_tests.tar'))

  def testMockError(self):
    """Test that a mock error does not execute logic, returns mocked value."""
    patch = self.PatchObject(test_service, 'BuildTargetUnitTest')

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    response = self._GetOutput()
    rc = test_controller.BuildTargetUnitTest(input_msg, response,
                                             self.mock_error_config)
    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(response.failed_packages)
    self.assertEqual(response.failed_packages[0].category, 'foo')
    self.assertEqual(response.failed_packages[0].package_name, 'bar')
    self.assertEqual(response.failed_packages[1].category, 'cat')
    self.assertEqual(response.failed_packages[1].package_name, 'pkg')

  def testNoArgumentFails(self):
    """Test no arguments fails."""
    input_msg = self._GetInput()
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg,
                                          self.api_config)

  def testNoBuildTargetFails(self):
    """Test missing build target name fails."""
    input_msg = self._GetInput(result_path=self.tempdir)
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg,
                                          self.api_config)

  def testNoResultPathFails(self):
    """Test missing result path fails."""
    # Missing result_path.
    input_msg = self._GetInput(board='board')
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg,
                                          self.api_config)

  def testPackageBuildFailure(self):
    """Test handling of raised BuildPackageFailure."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    pkgs = ['cat/pkg', 'foo/bar']
    expected = [('cat', 'pkg'), ('foo', 'bar')]

    result = test_service.BuildTargetUnitTestResult(1, None)
    result.failed_cpvs = [portage_util.SplitCPV(p, strict=False) for p in pkgs]
    self.PatchObject(test_service, 'BuildTargetUnitTest', return_value=result)

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg,
                                             self.api_config)

    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(output_msg.failed_packages)
    failed = []
    for pi in output_msg.failed_packages:
      failed.append((pi.category, pi.package_name))
    self.assertCountEqual(expected, failed)

  def testOtherBuildScriptFailure(self):
    """Test build script failure due to non-package emerge error."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    result = test_service.BuildTargetUnitTestResult(1, None)
    self.PatchObject(test_service, 'BuildTargetUnitTest', return_value=result)

    pkgs = ['foo/bar', 'cat/pkg']
    blacklist = [portage_util.SplitCPV(p, strict=False) for p in pkgs]
    input_msg = self._GetInput(board='board', result_path=self.tempdir,
                               empty_sysroot=True, blacklist=blacklist)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg,
                                             self.api_config)

    self.assertEqual(controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY, rc)
    self.assertFalse(output_msg.failed_packages)

  def testBuildTargetUnitTest(self):
    """Test BuildTargetUnitTest successful call."""
    input_msg = self._GetInput(board='board', result_path=self.tempdir)

    result = test_service.BuildTargetUnitTestResult(0, None)
    self.PatchObject(test_service, 'BuildTargetUnitTest', return_value=result)

    tarball_result = os.path.join(input_msg.result_path, 'unit_tests.tar')
    self.PatchObject(test_service, 'BuildTargetUnitTestTarball',
                     return_value=tarball_result)

    response = self._GetOutput()
    test_controller.BuildTargetUnitTest(input_msg, response,
                                        self.api_config)
    self.assertEqual(response.tarball_path,
                     os.path.join(input_msg.result_path, 'unit_tests.tar'))


class ChromiteUnitTestTest(cros_test_lib.MockTestCase,
                           api_config.ApiConfigMixin):
  """Tests for the ChromiteInfoTest function."""

  def setUp(self):
    self.board = 'board'
    self.chroot_path = '/path/to/chroot'

  def _GetInput(self, chroot_path=None):
    """Helper to build an input message instance."""
    proto = test_pb2.ChromiteUnitTestRequest(
        chroot={'path': chroot_path},
    )
    return proto

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return test_pb2.ChromiteUnitTestResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(cros_build_lib, 'run')

    input_msg = self._GetInput(chroot_path=self.chroot_path)
    test_controller.ChromiteUnitTest(input_msg, self._GetOutput(),
                                     self.validate_only_config)
    patch.assert_not_called()

  def testMockError(self):
    """Test mock error call does not execute any logic, returns error."""
    patch = self.PatchObject(cros_build_lib, 'run')

    input_msg = self._GetInput(chroot_path=self.chroot_path)
    rc = test_controller.ChromiteUnitTest(input_msg, self._GetOutput(),
                                          self.mock_error_config)
    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY, rc)

  def testMockCall(self):
    """Test mock call does not execute any logic, returns success."""
    patch = self.PatchObject(cros_build_lib, 'run')

    input_msg = self._GetInput(chroot_path=self.chroot_path)
    rc = test_controller.ChromiteUnitTest(input_msg, self._GetOutput(),
                                          self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_SUCCESS, rc)

  def testChromiteUnitTest(self):
    """Call ChromiteUnitTest with mocked cros_build_lib.run."""
    request = self._GetInput(chroot_path=self.chroot_path)
    patch = self.PatchObject(
        cros_build_lib, 'run',
        return_value=cros_build_lib.CommandResult(returncode=0))

    test_controller.ChromiteUnitTest(request, self._GetOutput(),
                                     self.api_config)
    patch.assert_called_once()


class CrosSigningTestTest(cros_test_lib.RunCommandTestCase,
                          api_config.ApiConfigMixin):
  """CrosSigningTest tests."""

  def setUp(self):
    self.chroot_path = '/path/to/chroot'

  def _GetInput(self, chroot_path=None):
    """Helper to build an input message instance."""
    proto = test_pb2.CrosSigningTestRequest(
        chroot={'path': chroot_path},
    )
    return proto

  def _GetOutput(self):
    """Helper to get an empty output message instance."""
    return test_pb2.CrosSigningTestResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    test_controller.CrosSigningTest(None, None, self.validate_only_config)
    self.assertFalse(self.rc.call_count)

  def testCrosSigningTest(self):
    """Call CrosSigningTest with mocked cros_build_lib.run."""
    request = self._GetInput(chroot_path=self.chroot_path)
    patch = self.PatchObject(
        cros_build_lib, 'run',
        return_value=cros_build_lib.CommandResult(returncode=0))

    test_controller.CrosSigningTest(request, self._GetOutput(),
                                    self.api_config)
    patch.assert_called_once()


class SimpleChromeWorkflowTestTest(cros_test_lib.MockTestCase,
                                   api_config.ApiConfigMixin):
  """Test the SimpleChromeWorkflowTest endpoint."""

  @staticmethod
  def _Output():
    return test_pb2.SimpleChromeWorkflowTestResponse()

  def _Input(self, sysroot_path=None, build_target=None, chrome_root=None,
             goma_config=None):
    proto = test_pb2.SimpleChromeWorkflowTestRequest()
    if sysroot_path:
      proto.sysroot.path = sysroot_path
    if build_target:
      proto.sysroot.build_target.name = build_target
    if chrome_root:
      proto.chrome_root = chrome_root
    if goma_config:
      proto.goma_config = goma_config
    return proto

  def setUp(self):
    self.chrome_path = 'path/to/chrome'
    self.sysroot_dir = 'build/board'
    self.build_target = 'amd64'
    self.mock_simple_chrome_workflow_test = self.PatchObject(
        test_service, 'SimpleChromeWorkflowTest')

  def testMissingBuildTarget(self):
    """Test SimpleChromeWorkflowTest dies when build_target not set."""
    input_proto = self._Input(build_target=None, sysroot_path='/sysroot/dir',
                              chrome_root='/chrome/path')
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.SimpleChromeWorkflowTest(input_proto, None,
                                               self.api_config)

  def testMissingSysrootPath(self):
    """Test SimpleChromeWorkflowTest dies when build_target not set."""
    input_proto = self._Input(build_target='board', sysroot_path=None,
                              chrome_root='/chrome/path')
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.SimpleChromeWorkflowTest(input_proto, None,
                                               self.api_config)

  def testMissingChromeRoot(self):
    """Test SimpleChromeWorkflowTest dies when build_target not set."""
    input_proto = self._Input(build_target='board', sysroot_path='/sysroot/dir',
                              chrome_root=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.SimpleChromeWorkflowTest(input_proto, None,
                                               self.api_config)

  def testSimpleChromeWorkflowTest(self):
    """Call SimpleChromeWorkflowTest with valid args and temp dir."""
    request = self._Input(sysroot_path='sysroot_path', build_target='board',
                          chrome_root='/path/to/chrome')
    response = self._Output()

    test_controller.SimpleChromeWorkflowTest(request, response, self.api_config)
    self.mock_simple_chrome_workflow_test.assert_called()

  def testValidateOnly(self):
    request = self._Input(sysroot_path='sysroot_path', build_target='board',
                          chrome_root='/path/to/chrome')
    test_controller.SimpleChromeWorkflowTest(request, self._Output(),
                                             self.validate_only_config)
    self.mock_simple_chrome_workflow_test.assert_not_called()


class VmTestTest(cros_test_lib.RunCommandTestCase, api_config.ApiConfigMixin):
  """Test the VmTest endpoint."""

  def _GetInput(self, **kwargs):
    values = dict(
        build_target=common_pb2.BuildTarget(name='target'),
        vm_path=common_pb2.Path(path='/path/to/image.bin',
                                location=common_pb2.Path.INSIDE),
        test_harness=test_pb2.VmTestRequest.TAST,
        vm_tests=[test_pb2.VmTestRequest.VmTest(pattern='suite')],
        ssh_options=test_pb2.VmTestRequest.SshOptions(
            port=1234, private_key_path={'path': '/path/to/id_rsa',
                                         'location': common_pb2.Path.INSIDE}),
    )
    values.update(kwargs)
    return test_pb2.VmTestRequest(**values)

  def _Output(self):
    return test_pb2.VmTestResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    test_controller.VmTest(self._GetInput(), None, self.validate_only_config)
    self.assertEqual(0, self.rc.call_count)

  def testTastAllOptions(self):
    """Test VmTest for Tast with all options set."""
    test_controller.VmTest(self._GetInput(), None, self.api_config)
    self.assertCommandContains([
        'cros_run_test', '--debug', '--no-display', '--copy-on-write',
        '--board', 'target',
        '--image-path', '/path/to/image.bin',
        '--tast', 'suite',
        '--ssh-port', '1234',
        '--private-key', '/path/to/id_rsa',
    ])

  def testAutotestAllOptions(self):
    """Test VmTest for Autotest with all options set."""
    input_proto = self._GetInput(test_harness=test_pb2.VmTestRequest.AUTOTEST)
    test_controller.VmTest(input_proto, None, self.api_config)
    self.assertCommandContains([
        'cros_run_test', '--debug', '--no-display', '--copy-on-write',
        '--board', 'target',
        '--image-path', '/path/to/image.bin',
        '--autotest', 'suite',
        '--ssh-port', '1234',
        '--private-key', '/path/to/id_rsa',
        '--test_that-args=--whitelist-chrome-crashes',
    ])

  def testMissingBuildTarget(self):
    """Test VmTest dies when build_target not set."""
    input_proto = self._GetInput(build_target=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None, self.api_config)

  def testMissingVmImage(self):
    """Test VmTest dies when vm_image not set."""
    input_proto = self._GetInput(vm_path=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None, self.api_config)

  def testMissingTestHarness(self):
    """Test VmTest dies when test_harness not specified."""
    input_proto = self._GetInput(
        test_harness=test_pb2.VmTestRequest.UNSPECIFIED)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None, self.api_config)

  def testMissingVmTests(self):
    """Test VmTest dies when vm_tests not set."""
    input_proto = self._GetInput(vm_tests=[])
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None, self.api_config)

  def testVmTest(self):
    """Call VmTest with valid args and temp dir."""
    request = self._GetInput()
    response = self._Output()
    patch = self.PatchObject(
        cros_build_lib, 'run',
        return_value=cros_build_lib.CommandResult(returncode=0))

    test_controller.VmTest(request, response, self.api_config)
    patch.assert_called()


class MoblabVmTestTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Test the MoblabVmTest endpoint."""

  @staticmethod
  def _Payload(path):
    return test_pb2.MoblabVmTestRequest.Payload(
        path=common_pb2.Path(path=path))

  @staticmethod
  def _Output():
    return test_pb2.MoblabVmTestResponse()

  def _Input(self):
    return test_pb2.MoblabVmTestRequest(
        chroot=common_pb2.Chroot(path=self.chroot_dir),
        image_payload=self._Payload(self.image_payload_dir),
        cache_payloads=[self._Payload(self.autotest_payload_dir)])

  def setUp(self):
    self.chroot_dir = '/chroot'
    self.chroot_tmp_dir = '/chroot/tmp'
    self.image_payload_dir = '/payloads/image'
    self.autotest_payload_dir = '/payloads/autotest'
    self.builder = 'moblab-generic-vm/R12-3.4.5-67.890'
    self.image_cache_dir = '/mnt/moblab/cache'
    self.image_mount_dir = '/mnt/image'

    self.PatchObject(chroot_lib.Chroot, 'tempdir', osutils.TempDir)

    self.mock_create_moblab_vms = self.PatchObject(
        test_service, 'CreateMoblabVm')
    self.mock_prepare_moblab_vm_image_cache = self.PatchObject(
        test_service, 'PrepareMoblabVmImageCache',
        return_value=self.image_cache_dir)
    self.mock_run_moblab_vm_tests = self.PatchObject(
        test_service, 'RunMoblabVmTest')
    self.mock_validate_moblab_vm_tests = self.PatchObject(
        test_service, 'ValidateMoblabVmTest')

    @contextlib.contextmanager
    def MockLoopbackPartitions(*_args, **_kwargs):
      mount = mock.MagicMock()
      mount.Mount.return_value = [self.image_mount_dir]
      yield mount

    self.PatchObject(image_lib, 'LoopbackPartitions', MockLoopbackPartitions)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    test_controller.MoblabVmTest(self._Input(), self._Output(),
                                 self.validate_only_config)
    self.mock_create_moblab_vms.assert_not_called()

  def testImageContainsBuilder(self):
    """MoblabVmTest calls service with correct args."""
    request = self._Input()
    response = self._Output()

    self.PatchObject(
        key_value_store, 'LoadFile',
        return_value={cros_set_lsb_release.LSB_KEY_BUILDER_PATH: self.builder})

    test_controller.MoblabVmTest(request, response, self.api_config)

    self.assertEqual(
        self.mock_create_moblab_vms.call_args_list,
        [mock.call(mock.ANY, self.chroot_dir, self.image_payload_dir)])
    self.assertEqual(
        self.mock_prepare_moblab_vm_image_cache.call_args_list,
        [mock.call(mock.ANY, self.builder, [self.autotest_payload_dir])])
    self.assertEqual(
        self.mock_run_moblab_vm_tests.call_args_list,
        [mock.call(mock.ANY, mock.ANY, self.builder, self.image_cache_dir,
                   mock.ANY)])
    self.assertEqual(
        self.mock_validate_moblab_vm_tests.call_args_list,
        [mock.call(mock.ANY)])

  def testImageMissingBuilder(self):
    """MoblabVmTest dies when builder path not found in lsb-release."""
    request = self._Input()
    response = self._Output()

    self.PatchObject(key_value_store, 'LoadFile', return_value={})

    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.MoblabVmTest(request, response, self.api_config)

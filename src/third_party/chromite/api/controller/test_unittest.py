# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The test controller tests."""

from __future__ import print_function

import mock

from chromite.api import controller
from chromite.api.controller import test as test_controller
from chromite.api.gen.chromiumos import common_pb2
from chromite.api.gen.chromite.api import image_pb2
from chromite.api.gen.chromite.api import test_pb2
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import osutils
from chromite.lib import portage_util


class BuildTargetUnitTestTest(cros_test_lib.MockTempDirTestCase):
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

  def testNoArgumentFails(self):
    """Test no arguments fails."""
    input_msg = self._GetInput()
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testNoBuildTargetFails(self):
    """Test missing build target name fails."""
    input_msg = self._GetInput(result_path=self.tempdir)
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testNoResultPathFails(self):
    """Test missing result path fails."""
    # Missing result_path.
    input_msg = self._GetInput(board='board')
    output_msg = self._GetOutput()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.BuildTargetUnitTest(input_msg, output_msg)

  def testPackageBuildFailure(self):
    """Test handling of raised BuildPackageFailure."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    pkgs = ['cat/pkg', 'foo/bar']
    expected = [('cat', 'pkg'), ('foo', 'bar')]
    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.PackageBuildFailure(rce, 'shortname', pkgs)
    self.PatchObject(commands, 'RunUnitTests', side_effect=error)

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(output_msg.failed_packages)
    failed = []
    for pi in output_msg.failed_packages:
      failed.append((pi.category, pi.package_name))
    self.assertItemsEqual(expected, failed)

  def testPopulatedEmergeFile(self):
    """Test build script failure due to using outside emerge status file."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    pkgs = ['cat/pkg', 'foo/bar']
    cpvs = [portage_util.SplitCPV(pkg, strict=False) for pkg in pkgs]
    expected = [('cat', 'pkg'), ('foo', 'bar')]
    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.BuildScriptFailure(rce, 'shortname')
    self.PatchObject(commands, 'RunUnitTests', side_effect=error)
    self.PatchObject(portage_util, 'ParseParallelEmergeStatusFile',
                     return_value=cpvs)

    input_msg = self._GetInput(board='board', result_path=self.tempdir)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    self.assertTrue(output_msg.failed_packages)
    failed = []
    for pi in output_msg.failed_packages:
      failed.append((pi.category, pi.package_name))
    self.assertItemsEqual(expected, failed)

  def testOtherBuildScriptFailure(self):
    """Test build script failure due to non-package emerge error."""
    tempdir = osutils.TempDir(base_dir=self.tempdir)
    self.PatchObject(osutils, 'TempDir', return_value=tempdir)

    rce = cros_build_lib.RunCommandError('error',
                                         cros_build_lib.CommandResult())
    error = failures_lib.BuildScriptFailure(rce, 'shortname')
    patch = self.PatchObject(commands, 'RunUnitTests', side_effect=error)
    self.PatchObject(portage_util, 'ParseParallelEmergeStatusFile',
                     return_value=[])

    pkgs = ['foo/bar', 'cat/pkg']
    blacklist = [portage_util.SplitCPV(p, strict=False) for p in pkgs]
    input_msg = self._GetInput(board='board', result_path=self.tempdir,
                               empty_sysroot=True, blacklist=blacklist)
    output_msg = self._GetOutput()

    rc = test_controller.BuildTargetUnitTest(input_msg, output_msg)

    self.assertEqual(controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY, rc)
    self.assertFalse(output_msg.failed_packages)
    patch.assert_called_with(constants.SOURCE_ROOT, 'board', extra_env=mock.ANY,
                             chroot_args=mock.ANY, build_stage=False,
                             blacklist=pkgs)


class VmTestTest(cros_test_lib.MockTestCase):
  """Test the VmTest endpoint."""

  def _GetInput(self, **kwargs):
    values = dict(
        build_target=common_pb2.BuildTarget(name='target'),
        vm_image=image_pb2.VmImage(path='/path/to/image.bin'),
        test_harness=test_pb2.VmTestRequest.TAST,
        vm_tests=[test_pb2.VmTestRequest.VmTest(pattern='suite')],
        ssh_options=test_pb2.VmTestRequest.SshOptions(
            port=1234, private_key_path='/path/to/id_rsa'),
    )
    values.update(kwargs)
    return test_pb2.VmTestRequest(**values)

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    self.StartPatcher(self.rc_mock)

  def testTastAllOptions(self):
    """Test VmTest for Tast with all options set."""
    test_controller.VmTest(self._GetInput(), None)
    self.rc_mock.assertCommandContains([
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
    test_controller.VmTest(input_proto, None)
    self.rc_mock.assertCommandContains([
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
      test_controller.VmTest(input_proto, None)

  def testMissingVmImage(self):
    """Test VmTest dies when vm_image not set."""
    input_proto = self._GetInput(vm_image=None)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None)

  def testMissingTestHarness(self):
    """Test VmTest dies when test_harness not specified."""
    input_proto = self._GetInput(
        test_harness=test_pb2.VmTestRequest.UNSPECIFIED)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None)

  def testMissingVmTests(self):
    """Test VmTest dies when vm_tests not set."""
    input_proto = self._GetInput(vm_tests=[])
    with self.assertRaises(cros_build_lib.DieSystemExit):
      test_controller.VmTest(input_proto, None)

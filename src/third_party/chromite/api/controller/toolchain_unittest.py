# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Toolchain-related operations."""

from __future__ import print_function

from chromite.api import api_config
from chromite.api.controller import toolchain
from chromite.api.gen.chromite.api import toolchain_pb2

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import toolchain_util


class UpdateEbuildWithAFDOArtifactsTest(cros_test_lib.MockTestCase,
                                        api_config.ApiConfigMixin):
  """Unittests for UpdateEbuildWithAFDOArtifacts."""

  @staticmethod
  def mock_die(message, *args):
    raise cros_build_lib.DieSystemExit(message % args)

  def setUp(self):
    self.board = 'board'
    self.response = toolchain_pb2.VerifyAFDOArtifactsResponse()
    self.invalid_artifact_type = toolchain_pb2.BENCHMARK_AFDO
    self.orderfile_command = self.PatchObject(
        toolchain_util, 'OrderfileUpdateChromeEbuild', return_value=True)
    self.kernel_command = self.PatchObject(
        toolchain_util, 'AFDOUpdateKernelEbuild', return_value=True)
    self.chrome_command = self.PatchObject(
        toolchain_util, 'AFDOUpdateChromeEbuild', return_value=True)
    self.PatchObject(cros_build_lib, 'Die', new=self.mock_die)

  def _GetRequest(self, build_target=None, artifact_type=None):
    return toolchain_pb2.VerifyAFDOArtifactsRequest(
        build_target={'name': build_target},
        artifact_type=artifact_type,
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mock value."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(self.response.status, True)

  def testWrongArtifactType(self):
    """Test passing wrong artifact type."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=self.invalid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                              self.api_config)
    self.assertIn('artifact_type (%d) must be in' % self.invalid_artifact_type,
                  str(context.exception))

  def testOrderfileSuccess(self):
    """Test the command is called correctly with orderfile."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.orderfile_command.assert_called_once_with(self.board)
    self.kernel_command.assert_not_called()
    self.chrome_command.assert_not_called()

  def testKernelAFDOSuccess(self):
    """Test the command is called correctly with kernel afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.KERNEL_AFDO)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.kernel_command.assert_called_once_with(self.board)
    self.orderfile_command.assert_not_called()
    self.chrome_command.assert_not_called()

  def testChromeAFDOSuccess(self):
    """Test the command is called correctly with Chrome afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.CHROME_AFDO)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.chrome_command.assert_called_once_with(self.board)
    self.orderfile_command.assert_not_called()
    self.kernel_command.assert_not_called()


class UploadVettedFDOArtifactsTest(UpdateEbuildWithAFDOArtifactsTest):
  """Unittests for UploadVettedAFDOArtifacts."""

  @staticmethod
  def mock_die(message, *args):
    raise cros_build_lib.DieSystemExit(message % args)

  def setUp(self):
    self.board = 'board'
    self.response = toolchain_pb2.VerifyAFDOArtifactsResponse()
    self.invalid_artifact_type = toolchain_pb2.BENCHMARK_AFDO
    self.command = self.PatchObject(
        toolchain_util,
        'UploadAndPublishVettedAFDOArtifacts',
        return_value=True)
    self.PatchObject(cros_build_lib, 'Die', new=self.mock_die)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                        self.validate_only_config)
    self.command.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mock value."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                        self.mock_call_config)
    self.command.assert_not_called()
    self.assertEqual(self.response.status, True)

  def testWrongArtifactType(self):
    """Test passing wrong artifact type."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=self.invalid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                          self.api_config)
    self.assertIn('artifact_type (%d) must be in' % self.invalid_artifact_type,
                  str(context.exception))

  def testOrderfileSuccess(self):
    """Test the command is called correctly with orderfile."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('orderfile', self.board)

  def testKernelAFDOSuccess(self):
    """Test the command is called correctly with kernel afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.KERNEL_AFDO)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('kernel_afdo', self.board)

  def testChromeAFDOSuccess(self):
    """Test the command is called correctly with Chrome afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.CHROME_AFDO)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('chrome_afdo', self.board)

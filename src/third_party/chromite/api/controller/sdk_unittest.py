# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SDK tests."""

from __future__ import print_function

import mock

from chromite.api import api_config
from chromite.api.controller import sdk as sdk_controller
from chromite.api.gen.chromite.api import sdk_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.service import sdk as sdk_service


class SdkCreateTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Create tests."""

  def setUp(self):
    """Setup method."""
    # We need to run the command outside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.response = sdk_pb2.CreateResponse()

  def _GetRequest(self, no_replace=False, bootstrap=False, no_use_image=False,
                  cache_path=None, chroot_path=None):
    """Helper to build a create request message."""
    request = sdk_pb2.CreateRequest()
    request.flags.no_replace = no_replace
    request.flags.bootstrap = bootstrap
    request.flags.no_use_image = no_use_image

    if cache_path:
      request.chroot.cache_dir = cache_path
    if chroot_path:
      request.chroot.path = chroot_path

    return request

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sdk_service, 'Create')

    sdk_controller.Create(self._GetRequest(), self.response,
                          self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sdk_service, 'Create')

    rc = sdk_controller.Create(self._GetRequest(), self.response,
                               self.mock_call_config)
    patch.assert_not_called()
    self.assertFalse(rc)
    self.assertTrue(self.response.version.version)

  def testSuccess(self):
    """Test the successful call output handling."""
    self.PatchObject(sdk_service, 'Create', return_value=1)

    request = self._GetRequest()

    sdk_controller.Create(request, self.response, self.api_config)

    self.assertEqual(1, self.response.version.version)

  def testFalseArguments(self):
    """Test False argument handling."""
    # Create the patches.
    self.PatchObject(sdk_service, 'Create', return_value=1)
    args_patch = self.PatchObject(sdk_service, 'CreateArguments')

    # Flag translation tests.
    # Test all false values in the message.
    request = self._GetRequest(no_replace=False, bootstrap=False,
                               no_use_image=False)
    sdk_controller.Create(request, self.response, self.api_config)
    args_patch.assert_called_with(replace=True, bootstrap=False,
                                  use_image=True, paths=mock.ANY)

  def testTrueArguments(self):
    """Test True arguments handling."""
    # Create the patches.
    self.PatchObject(sdk_service, 'Create', return_value=1)
    args_patch = self.PatchObject(sdk_service, 'CreateArguments')

    # Test all True values in the message.
    request = self._GetRequest(no_replace=True, bootstrap=True,
                               no_use_image=True)
    sdk_controller.Create(request, self.response, self.api_config)
    args_patch.assert_called_with(replace=False, bootstrap=True,
                                  use_image=False, paths=mock.ANY)

  def testPathArguments(self):
    """Test the path arguments handling."""
    # Create the patches.
    self.PatchObject(sdk_service, 'Create', return_value=1)
    paths_patch = self.PatchObject(sdk_service, 'ChrootPaths')

    # Test the path arguments get passed through.
    cache_dir = '/cache/dir'
    chroot_path = '/chroot/path'
    request = self._GetRequest(cache_path=cache_dir, chroot_path=chroot_path)
    sdk_controller.Create(request, self.response, self.api_config)
    paths_patch.assert_called_with(cache_dir=cache_dir, chroot_path=chroot_path)


class SdkUpdateTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Update tests."""

  def setUp(self):
    """Setup method."""
    # We need to run the command inside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

    self.response = sdk_pb2.UpdateResponse()

  def _GetRequest(self, build_source=False, targets=None):
    """Helper to simplify building a request instance."""
    request = sdk_pb2.UpdateRequest()
    request.flags.build_source = build_source

    for target in targets or []:
      added = request.toolchain_targets.add()
      added.name = target

    return request

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sdk_service, 'Update')

    sdk_controller.Update(self._GetRequest(), self.response,
                          self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(sdk_service, 'Update')

    rc = sdk_controller.Create(self._GetRequest(), self.response,
                               self.mock_call_config)
    patch.assert_not_called()
    self.assertFalse(rc)
    self.assertTrue(self.response.version.version)

  def testSuccess(self):
    """Successful call output handling test."""
    expected_version = 1
    self.PatchObject(sdk_service, 'Update', return_value=expected_version)
    request = self._GetRequest()

    sdk_controller.Update(request, self.response, self.api_config)

    self.assertEqual(expected_version, self.response.version.version)

  def testArgumentHandling(self):
    """Test the proto argument handling."""
    args = sdk_service.UpdateArguments()
    self.PatchObject(sdk_service, 'Update', return_value=1)
    args_patch = self.PatchObject(sdk_service, 'UpdateArguments',
                                  return_value=args)

    # No boards and flags False.
    request = self._GetRequest(build_source=False)
    sdk_controller.Update(request, self.response, self.api_config)
    args_patch.assert_called_with(build_source=False, toolchain_targets=[])

    # Multiple boards and flags True.
    targets = ['board1', 'board2']
    request = self._GetRequest(build_source=True, targets=targets)
    sdk_controller.Update(request, self.response, self.api_config)
    args_patch.assert_called_with(build_source=True, toolchain_targets=targets)

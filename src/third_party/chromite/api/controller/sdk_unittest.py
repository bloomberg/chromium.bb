# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SDK tests."""

from __future__ import print_function

from chromite.lib import cros_test_lib

from chromite.api.controller import sdk as sdk_controller
from chromite.api.gen.chromite.api import sdk_pb2
from chromite.lib import cros_build_lib
from chromite.service import sdk as sdk_service


class SdkCreateTest(cros_test_lib.MockTestCase):
  """Create tests."""

  def setUp(self):
    """Setup method."""
    # We need to run the command outside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)

  def _GetRequest(self, no_replace=False, bootstrap=False, no_use_image=False,
                  cache_path=None, chrome_path=None, chroot_path=None):
    """Helper to build a create request message."""
    request = sdk_pb2.CreateRequest()
    request.flags.no_replace = no_replace
    request.flags.bootstrap = bootstrap
    request.flags.no_use_image = no_use_image

    if cache_path:
      request.paths.cache = cache_path
    if chrome_path:
      request.paths.chrome = chrome_path
    if chroot_path:
      request.paths.chroot = chroot_path

    return request

  def testSuccess(self):
    """Test the successful call output handling."""
    self.PatchObject(sdk_service, 'Create', return_value=1)

    request = self._GetRequest()
    response = sdk_pb2.CreateResponse()

    sdk_controller.Create(request, response)

    self.assertEqual(1, response.version.version)

  def testArgumentHandling(self):
    """Test the argument handling."""
    # Get some defaults to return so it's passing around valid objects.
    paths_obj = sdk_service.ChrootPaths()
    args_obj = sdk_service.CreateArguments()

    # Create the patches.
    self.PatchObject(sdk_service, 'Create', return_value=1)
    paths_patch = self.PatchObject(sdk_service, 'ChrootPaths',
                                   return_value=paths_obj)
    args_patch = self.PatchObject(sdk_service, 'CreateArguments',
                                  return_value=args_obj)

    # Just need a response to pass through.
    response = sdk_pb2.CreateResponse()

    # Flag translation tests.
    # Test all false values in the message.
    request = self._GetRequest(no_replace=False, bootstrap=False,
                               no_use_image=False)
    sdk_controller.Create(request, response)
    args_patch.assert_called_with(replace=True, bootstrap=False,
                                  use_image=True, paths=paths_obj)

    # Test all True values in the message.
    request = self._GetRequest(no_replace=True, bootstrap=True,
                               no_use_image=True)
    sdk_controller.Create(request, response)
    args_patch.assert_called_with(replace=False, bootstrap=True,
                                  use_image=False, paths=paths_obj)

    # Test the path arguments get passed through.
    cache_dir = '/cache/dir'
    chrome_root = '/chrome/path'
    chroot_path = '/chroot/path'
    request = self._GetRequest(cache_path=cache_dir, chrome_path=chrome_root,
                               chroot_path=chroot_path)
    sdk_controller.Create(request, response)
    paths_patch.assert_called_with(cache_dir=cache_dir, chrome_root=chrome_root,
                                   chroot_path=chroot_path)


class SdkUpdateTest(cros_test_lib.MockTestCase):
  """Update tests."""

  def setUp(self):
    """Setup method."""
    # We need to run the command inside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

  def _GetRequest(self, build_source=False, targets=None):
    """Helper to simplify building a request instance."""
    request = sdk_pb2.UpdateRequest()
    request.flags.build_source = build_source

    for target in targets or []:
      added = request.toolchain_targets.add()
      added.name = target

    return request

  def _GetResponse(self):
    """Helper to build an empty response instance."""
    return sdk_pb2.UpdateResponse()

  def testSuccess(self):
    """Successful call output handling test."""
    expected_version = 1
    self.PatchObject(sdk_service, 'Update', return_value=expected_version)
    request = self._GetRequest()
    response = self._GetResponse()

    sdk_controller.Update(request, response)

    self.assertEqual(expected_version, response.version.version)

  def testArgumentHandling(self):
    """Test the proto argument handling."""
    args = sdk_service.UpdateArguments()
    self.PatchObject(sdk_service, 'Update', return_value=1)
    args_patch = self.PatchObject(sdk_service, 'UpdateArguments',
                                  return_value=args)

    response = self._GetResponse()

    # No boards and flags False.
    request = self._GetRequest(build_source=False)
    sdk_controller.Update(request, response)
    args_patch.assert_called_with(build_source=False, toolchain_targets=[])

    # Multiple boards and flags True.
    targets = ['board1', 'board2']
    request = self._GetRequest(build_source=True, targets=targets)
    sdk_controller.Update(request, response)
    args_patch.assert_called_with(build_source=True, toolchain_targets=targets)

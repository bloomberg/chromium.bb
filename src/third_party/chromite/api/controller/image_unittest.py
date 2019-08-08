# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image service tests."""

from __future__ import print_function

import mock
import os

from chromite.api import controller
from chromite.api.controller import image as image_controller
from chromite.api.gen.chromite.api import image_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.service import image as image_service


class CreateTest(cros_test_lib.MockTempDirTestCase):
  """Create image tests."""

  def _GetRequest(self, board=None, types=None, version=None, builder_path=None,
                  disable_rootfs_verification=False):
    """Helper to build a request instance."""
    return image_pb2.CreateImageRequest(
        build_target={'name': board},
        image_types=types,
        disable_rootfs_verification=disable_rootfs_verification,
        version=version,
        builder_path=builder_path,
    )

  def _GetResponse(self):
    """Helper to build an empty response instance."""
    return image_pb2.CreateImageResult()

  def testArgumentValidation(self):
    """Test the argument validation."""
    input_proto = image_pb2.CreateImageRequest()
    output_proto = image_pb2.CreateImageResult()

    # No board should cause it to fail.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Create(input_proto, output_proto)

  def testNoTypeSpecified(self):
    """Test the image type default."""
    request = self._GetRequest(board='board')
    response = self._GetResponse()

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, response)
    build_patch.assert_called_with(images=[constants.IMAGE_TYPE_BASE],
                                   board='board', config=mock.ANY)

  def testSingleTypeSpecified(self):
    """Test it's properly using a specified type."""
    request = self._GetRequest(board='board', types=[common_pb2.DEV])
    response = self._GetResponse()

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, response)
    build_patch.assert_called_with(images=[constants.IMAGE_TYPE_DEV],
                                   board='board', config=mock.ANY)

  def testMultipleAndImpliedTypes(self):
    """Test multiple types and implied type handling."""
    # The TEST_VM type should force it to build the test image.
    types = [common_pb2.BASE, common_pb2.TEST_VM]
    expected_images = [constants.IMAGE_TYPE_BASE, constants.IMAGE_TYPE_TEST]

    request = self._GetRequest(board='board', types=types)
    response = self._GetResponse()

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, response)
    build_patch.assert_called_with(images=expected_images, board='board',
                                   config=mock.ANY)

  def testFailedPackageHandling(self):
    """Test failed packages are populated correctly."""
    result = image_service.BuildResult(1, ['foo/bar', 'cat/pkg'])
    expected_packages = [('foo', 'bar'), ('cat', 'pkg')]
    self.PatchObject(image_service, 'Build', return_value=result)

    input_proto = image_pb2.CreateImageRequest()
    input_proto.build_target.name = 'board'
    output_proto = image_pb2.CreateImageResult()

    rc = image_controller.Create(input_proto, output_proto)
    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    for package in output_proto.failed_packages:
      self.assertIn((package.category, package.package_name), expected_packages)


class CreateVmTest(cros_test_lib.MockTestCase):
  """CreateVm tests."""

  def _GetInput(self, board=None, image_type=None):
    """Helper to create an input proto instance."""
    # pylint: disable=protected-access

    return image_pb2.CreateVmRequest(
        image={'build_target': {'name': board}, 'type': image_type})

  def _GetOutput(self):
    """Helper to create an empty output proto instance."""
    return image_pb2.CreateVmResponse()

  def testNoArgsFails(self):
    """Make sure it fails with no arguments."""
    request = self._GetInput()
    response = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.CreateVm(request, response)

  def testNoBuildTargetFails(self):
    """Make sure it fails with no build target."""
    request = self._GetInput(image_type=common_pb2.TEST)
    response = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.CreateVm(request, response)

  def testNoTypeFails(self):
    """Make sure it fails with no build target."""
    request = self._GetInput(board='board')
    response = self._GetOutput()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.CreateVm(request, response)

  def testTestImage(self):
    """Make sure the test image identification works properly."""
    request = self._GetInput(board='board', image_type=common_pb2.TEST)
    response = self._GetOutput()
    create_patch = self.PatchObject(image_service, 'CreateVm',
                                    return_value='/vm/path')

    image_controller.CreateVm(request, response)

    create_patch.assert_called_once_with('board', chroot=mock.ANY, is_test=True)

  def testNonTestImage(self):
    """Make sure the test image identification works properly."""
    request = self._GetInput(board='board', image_type=common_pb2.BASE)
    response = self._GetOutput()
    create_patch = self.PatchObject(image_service, 'CreateVm',
                                    return_value='/vm/path')

    image_controller.CreateVm(request, response)

    create_patch.assert_called_once_with('board', chroot=mock.ANY,
                                         is_test=False)


class ImageTest(cros_test_lib.MockTempDirTestCase):
  """Image service tests."""

  def setUp(self):
    self.image_path = os.path.join(self.tempdir, 'image.bin')
    self.board = 'board'
    self.result_directory = os.path.join(self.tempdir, 'results')

    osutils.SafeMakedirs(self.result_directory)
    osutils.Touch(self.image_path)

  def testTestArgumentValidation(self):
    """Test function argument validation tests."""
    self.PatchObject(image_service, 'Test', return_value=True)
    input_proto = image_pb2.TestImageRequest()
    output_proto = image_pb2.TestImageResult()

    # Nothing provided.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto)

    # Just one argument.
    input_proto.build_target.name = self.board
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto)

    # Two arguments provided.
    input_proto.result.directory = self.result_directory
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto)

    # Invalid image path.
    input_proto.image.path = '/invalid/image/path'
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto)

    # All valid arguments.
    input_proto.image.path = self.image_path
    image_controller.Test(input_proto, output_proto)

  def testTestOutputHandling(self):
    """Test function output tests."""
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    input_proto.build_target.name = self.board
    input_proto.result.directory = self.result_directory
    output_proto = image_pb2.TestImageResult()

    self.PatchObject(image_service, 'Test', return_value=True)
    image_controller.Test(input_proto, output_proto)
    self.assertTrue(output_proto.success)

    self.PatchObject(image_service, 'Test', return_value=False)
    image_controller.Test(input_proto, output_proto)
    self.assertFalse(output_proto.success)

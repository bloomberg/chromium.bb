# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image API Service.

The image related API endpoints should generally be found here.
"""

from __future__ import print_function

import os

from chromite.api.gen.chromite.api import image_pb2
from chromite.lib import constants
from chromite.lib import image_lib
from chromite.service import image

# The image.proto ImageType enum ids.
_BASE_ID = image_pb2.Image.BASE
_DEV_ID = image_pb2.Image.DEV
_TEST_ID = image_pb2.Image.TEST

# Dict to allow easily translating names to enum ids and vice versa.
_IMAGE_MAPPING = {
    _BASE_ID: constants.IMAGE_TYPE_BASE,
    constants.IMAGE_TYPE_BASE: _BASE_ID,
    _DEV_ID: constants.IMAGE_TYPE_DEV,
    constants.IMAGE_TYPE_DEV: _DEV_ID,
    _TEST_ID: constants.IMAGE_TYPE_TEST,
    constants.IMAGE_TYPE_TEST: _TEST_ID,
}


class Error(Exception):
  """The module's base error class."""


class InvalidImageTypeError(Error):
  """Invalid image type given."""


class InvalidArgumentError(Error):
  """Invalid argument to an image service function."""


def Create(input_proto, output_proto):
  """Build an image.

  Args:
    input_proto (image_pb2.CreateImageRequest): The input message.
    output_proto (image_pb2.CreateImageResult): The output message.
  """
  board = input_proto.build_target.name
  if not board:
    raise InvalidArgumentError('build_target.name is required.')

  image_types = set()
  # Build the base image if no images provided.
  to_build = input_proto.image_types or [_BASE_ID]
  for current in to_build:
    if current not in _IMAGE_MAPPING:
      # Not expected, but at least it will be obvious if this comes up.
      raise InvalidImageTypeError(
          "The service's known image types do not match those in image.proto. "
          'Unknown Enum ID: %s' % current)

    image_types.add(_IMAGE_MAPPING[current])

  enable_rootfs_verification = not input_proto.disable_rootfs_verification
  version = input_proto.version or None
  disk_layout = input_proto.disk_layout or None
  builder_path = input_proto.builder_path or None
  build_config = image.BuildConfig(
      enable_rootfs_verification=enable_rootfs_verification, replace=True,
      version=version, disk_layout=disk_layout, builder_path=builder_path,
  )

  # Sorted isn't really necessary here, but it's much easier to test.
  result = image.Build(board=board, images=sorted(list(image_types)),
                       config=build_config)

  output_proto.success = result.success
  if result.success:
    # Success -- we need to list out the images we built in the output.
    _PopulateBuiltImages(board, image_types, output_proto)
  else:
    # Failure -- include all of the failed packages in the output.
    for package in result.failed_packages:
      current = output_proto.failed_packages.add()
      current.category = package.category
      current.package_name = package.package
      if package.version:
        current.version = package.version

    return 1


def _PopulateBuiltImages(board, image_types, output_proto):
  """Helper to list out built images for Create."""
  # Build out the ImageType->ImagePath mapping in the output.
  # We're using the default path, so just fetch that, but read the symlink so
  # the path we're returning is somewhat more permanent.
  latest_link = image_lib.GetLatestImageLink(board)
  read_link = os.readlink(latest_link)
  if os.path.isabs(read_link):
    # Absolute path, use the linked location.
    base_path = os.path.normpath(read_link)
  else:
    # Relative path, convert to absolute using the symlink's containing folder.
    base_path = os.path.join(os.path.dirname(latest_link), read_link)
    base_path = os.path.normpath(base_path)

  for current in image_types:
    type_id = _IMAGE_MAPPING[current]
    path = os.path.join(base_path, constants.IMAGE_TYPE_TO_NAME[current])

    new_image = output_proto.images.add()
    new_image.path = path
    new_image.type = type_id


def Test(input_proto, output_proto):
  """Run image tests.

  Args:
    input_proto (image_pb2.ImageTestRequest): The input message.
    output_proto (image_pb2.ImageTestResult): The output message.
  """
  image_path = input_proto.image.path
  board = input_proto.build_target.name
  result_directory = input_proto.result.directory

  if not board:
    raise InvalidArgumentError('The build_target.name is required.')
  if not result_directory:
    raise InvalidArgumentError('The result.directory is required.')
  if not image_path:
    raise InvalidArgumentError('The image.path is required.')

  if not os.path.isfile(image_path) or not image_path.endswith('.bin'):
    raise InvalidArgumentError(
        'The image.path must be an existing image file with a .bin extension.')

  output_proto.success = image.Test(board, result_directory,
                                    image_dir=image_path)

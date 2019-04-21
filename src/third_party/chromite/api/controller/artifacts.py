# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements ArtifactService."""

from __future__ import print_function

import os

from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils


def _GetImageDir(build_root, target):
  """Return path containing images for the given build target.

  Args:
    build_root (str): Path to checkout where build occurs.
    target (str): Name of the build target.

  Returns:
    Path to the directory containing target images.

  Raises:
    DieSystemExit: If the image dir does not exist.
  """
  image_dir = os.path.join(build_root, 'src/build/images', target, 'latest')
  if not os.path.exists(image_dir):
    cros_build_lib.Die('Expected to find image output for target %s at %s, '
                       'but path does not exist' % (target, image_dir))
  return image_dir


def BundleImageZip(input_proto, output_proto):
  """Bundle image.zip.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  image_dir = _GetImageDir(constants.SOURCE_ROOT, target)
  archive = commands.BuildImageZip(output_dir, image_dir)
  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleTestUpdatePayloads(input_proto, output_proto):
  """Generate minimal update payloads for the build target for testing.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  # Use the first available image to create the update payload.
  img_dir = _GetImageDir(build_root, target)
  img_types = [
      constants.IMAGE_TYPE_TEST, constants.IMAGE_TYPE_DEV,
      constants.IMAGE_TYPE_BASE
  ]
  img_paths = []
  for img_type in img_types:
    img_path = os.path.join(img_dir, constants.IMAGE_TYPE_TO_NAME[img_type])
    if os.path.exists(img_path):
      img_paths.append(img_path)

  if not img_paths:
    cros_build_lib.Die(
        'Expected to find an image of type among %r for target "%s" '
        'at path %s.', img_types, target, img_dir)
  img = img_paths[0]

  # Unfortunately, the relevant commands.py functions do not return
  # a list of generated files. As a workaround, we have commands.py
  # put the files in a separate temporary directory so we can catalog them,
  # then move them to the output dir.
  # TODO(saklein): Repalce with a chromite/service implementation.
  with osutils.TempDir() as temp:
    commands.GeneratePayloads(img, temp, full=True, stateful=True, delta=True)
    commands.GenerateQuickProvisionPayloads(img, temp)
    for path in osutils.DirectoryIterator(temp):
      if os.path.isfile(path):
        rel_path = os.path.relpath(path, temp)
        output_proto.artifacts.add().path = os.path.join(output_dir, rel_path)
    osutils.CopyDirContents(temp, output_dir, allow_nonempty=True)


def BundleAutotestFiles(input_proto, output_proto):
  """Tar the autotest files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT
  cwd = os.path.join(build_root, 'chroot/build', target, 'usr/local/build')

  # Note that unlike the functions below, this returns the full path
  # to *multiple* tarballs.
  # TODO(saklein): Replace with a chromite/service implementation.
  archives = commands.BuildAutotestTarballsForHWTest(build_root, cwd,
                                                     output_dir)

  for archive in archives:
    output_proto.artifacts.add().path = archive


def BundleTastFiles(input_proto, output_proto):
  """Tar the tast files for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT
  cwd = os.path.join(build_root, 'chroot/build', target, 'build')

  # Note that unlike the functions below, this returns the full path
  # to the tarball.
  # TODO(saklein): Replace with a chromite/service implementation.
  archive = commands.BuildTastBundleTarball(build_root, cwd, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not bundle Tast files. '
        'No Tast directories found for %s.', target)

  output_proto.artifacts.add().path = archive


def BundlePinnedGuestImages(input_proto, output_proto):
  """Tar the pinned guest images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  # TODO(saklein): Replace with a chromite/service implementation.
  archive = commands.BuildPinnedGuestImagesTarball(build_root, target,
                                                   output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not bundle pinned guest images. '
        'No relevant files found for %s.', target)

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleFirmware(input_proto, output_proto):
  """Tar the firmware images for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir
  build_root = constants.SOURCE_ROOT

  # TODO(saklein): Replace with a chromite/service implementation.
  archive = commands.BuildFirmwareArchive(build_root, target, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not create firmware archive. No firmware found for %s.', target)

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)


def BundleEbuildLogs(input_proto, output_proto):
  """Tar the ebuild logs for a build target.

  Args:
    input_proto (BundleRequest): The input proto.
    output_proto (BundleRequest): The output proto.
  """
  target = input_proto.build_target.name
  output_dir = input_proto.output_dir

  # commands.BuildEbuildLogsTarball conflates "buildroot" with sysroot.
  # Adjust accordingly...
  build_root = os.path.join(constants.SOURCE_ROOT, 'chroot', 'build')

  # TODO(saklein): Replace with a chromite/service implementation.
  archive = commands.BuildEbuildLogsTarball(build_root, target, output_dir)

  if archive is None:
    cros_build_lib.Die(
        'Could not create ebuild logs archive. No logs found for %s.', target)

  output_proto.artifacts.add().path = os.path.join(output_dir, archive)

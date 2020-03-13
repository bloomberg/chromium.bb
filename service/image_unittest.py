# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image API unittests."""

from __future__ import print_function

import os
import sys

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.service import image


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class BuildImageTest(cros_test_lib.RunCommandTempDirTestCase):
  """Build Image tests."""

  def setUp(self):
    osutils.Touch(os.path.join(self.tempdir,
                               image.PARALLEL_EMERGE_STATUS_FILE_NAME))
    self.PatchObject(osutils.TempDir, '__enter__', return_value=self.tempdir)

  def testInsideChrootCommand(self):
    """Test the build_image command when called from inside the chroot."""
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    image.Build(board='board')
    self.assertCommandContains(
        [os.path.join(constants.CROSUTILS_DIR, 'build_image')])

  def testOutsideChrootCommand(self):
    """Test the build_image command when called from outside the chroot."""
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    image.Build(board='board')
    self.assertCommandContains(['./build_image'])

  def testBuildBoardHandling(self):
    """Test the argument handling."""
    # No board and no default should raise an error.
    self.PatchObject(cros_build_lib, 'GetDefaultBoard', return_value=None)
    with self.assertRaises(image.InvalidArgumentError):
      image.Build()

    # Falls back to default when no board provided.
    self.PatchObject(cros_build_lib, 'GetDefaultBoard', return_value='default')
    image.Build()
    self.assertCommandContains(['--board', 'default'])

    # Should be using the passed board before the default.
    image.Build('board')
    self.assertCommandContains(['--board', 'board'])

  def testBuildImageTypes(self):
    """Test the image type handling."""
    # Should default to building the base image.
    image.Build('board')
    self.assertCommandContains([constants.IMAGE_TYPE_BASE])

    # Should be using the argument when passed.
    image.Build('board', [constants.IMAGE_TYPE_DEV])
    self.assertCommandContains([constants.IMAGE_TYPE_DEV])

    # Multiple should all be passed.
    multi = [constants.IMAGE_TYPE_BASE, constants.IMAGE_TYPE_DEV,
             constants.IMAGE_TYPE_TEST]
    image.Build('board', multi)
    self.assertCommandContains(multi)


class BuildConfigTest(cros_test_lib.MockTestCase):
  """BuildConfig tests."""

  def testGetArguments(self):
    """GetArguments tests."""
    config = image.BuildConfig()
    self.assertEqual([], config.GetArguments())

    # Make sure each arg produces the correct argument individually.
    config.builder_path = 'test'
    self.assertEqual(['--builder_path', 'test'], config.GetArguments())
    config.builder_path = None

    config.disk_layout = 'disk'
    self.assertEqual(['--disk_layout', 'disk'], config.GetArguments())
    config.disk_layout = None

    config.enable_rootfs_verification = False
    self.assertEqual(['--noenable_rootfs_verification'], config.GetArguments())
    config.enable_rootfs_verification = True

    config.replace = True
    self.assertEqual(['--replace'], config.GetArguments())
    config.replace = False

    config.version = 'version'
    self.assertEqual(['--version', 'version'], config.GetArguments())
    config.version = None


class CreateVmTest(cros_test_lib.RunCommandTestCase):
  """Create VM tests."""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

  def testNoBoardFails(self):
    """Should fail when not given a valid board-ish value."""
    with self.assertRaises(AssertionError):
      image.CreateVm('')

  def testBoardArgument(self):
    """Test the board argument."""
    image.CreateVm('board')
    self.assertCommandContains(['--board', 'board'])

  def testTestImage(self):
    """Test the application of the --test_image argument."""
    image.CreateVm('board', is_test=True)
    self.assertCommandContains(['--test_image'])

  def testNonTestImage(self):
    """Test the non-application of the --test_image argument."""
    image.CreateVm('board', is_test=False)
    self.assertCommandContains(['--test_image'], expected=False)

  def testCommandError(self):
    """Test handling of an error when running the command."""
    self.rc.SetDefaultCmdResult(returncode=1)
    with self.assertRaises(image.ImageToVmError):
      image.CreateVm('board')

  def testResultPath(self):
    """Test the path building."""
    self.PatchObject(image_lib, 'GetLatestImageLink', return_value='/tmp')
    self.assertEqual(os.path.join('/tmp', constants.VM_IMAGE_BIN),
                     image.CreateVm('board'))


class ImageTestTest(cros_test_lib.RunCommandTempDirTestCase):
  """Image Test tests."""

  def setUp(self):
    """Setup the filesystem."""
    self.board = 'board'
    self.chroot_container = os.path.join(self.tempdir, 'outside')
    self.outside_result_dir = os.path.join(self.chroot_container, 'results')
    self.inside_result_dir_inside = '/inside/results_inside'
    self.inside_result_dir_outside = os.path.join(self.chroot_container,
                                                  'inside/results_inside')
    self.image_dir_inside = '/inside/build/board/latest'
    self.image_dir_outside = os.path.join(self.chroot_container,
                                          'inside/build/board/latest')

    D = cros_test_lib.Directory
    filesystem = (
        D('outside', (
            D('results', ()),
            D('inside', (
                D('results_inside', ()),
                D('build', (
                    D('board', (
                        D('latest', ('%s.bin' % constants.BASE_IMAGE_NAME,)),
                    )),
                )),
            )),
        )),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

  def testTestFailsInvalidArguments(self):
    """Test invalid arguments are correctly failed."""
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(None, None)
    with self.assertRaises(image.InvalidArgumentError):
      image.Test('', '')
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(None, self.outside_result_dir)
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(self.board, None)

  def testTestInsideChrootAllProvided(self):
    """Test behavior when inside the chroot and all paths provided."""
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    image.Test(self.board, self.outside_result_dir,
               image_dir=self.image_dir_inside)

    # Inside chroot shouldn't need to do any path manipulations, so we should
    # see exactly what we called it with.
    self.assertCommandContains(['--board', self.board,
                                '--test_results_root', self.outside_result_dir,
                                self.image_dir_inside])

  def testTestInsideChrootNoImageDir(self):
    """Test image dir generation inside the chroot."""
    mocked_dir = '/foo/bar'
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.PatchObject(image_lib, 'GetLatestImageLink', return_value=mocked_dir)
    image.Test(self.board, self.outside_result_dir)

    self.assertCommandContains(['--board', self.board,
                                '--test_results_root', self.outside_result_dir,
                                mocked_dir])

# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the image_lib module."""

from __future__ import print_function

import gc
import glob
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import partial_mock


class FakeException(Exception):
  """Fake exception used for testing exception handling."""


class LoopbackPartitions(object):
  """Mocked loopback partition class to use in unit tests.

  Args:
    path: Path to the image file.
    dev: Path for the base loopback device.
    part_count: How many partition device files to make up.
    part_overrides: A dict which is used to update self.parts.
  """
  # pylint: disable=dangerous-default-value
  def __init__(self, path='/dev/loop9999', dev=None,
               part_count=None, part_overrides={}):
    self.path = path
    self.dev = dev
    self.parts = {}
    for i in xrange(part_count):
      self.parts[i + 1] = path + 'p' + str(i + 1)
    self.parts.update(part_overrides)

  def close(self):
    pass

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc, tb):
    pass


FAKE_PATH = '/imaginary/file'
LOOP_DEV = '/dev/loop9999'
LOOP_PARTS_DICT = {num: '%sp%d' % (LOOP_DEV, num) for num in range(1, 13)}
LOOP_PARTS_LIST = LOOP_PARTS_DICT.values()

class LoopbackPartitionsTest(cros_test_lib.MockTestCase):
  """Test the loopback partitions class"""

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.StartPatcher(self.rc_mock)
    self.rc_mock.SetDefaultCmdResult()

    self.PatchObject(glob, 'glob', return_value=LOOP_PARTS_LIST)
    def fake_which(val, *_arg, **_kwargs):
      return val
    self.PatchObject(osutils, 'Which', side_effect=fake_which)

  def testContextManager(self):
    """Test using the loopback class as a context manager."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    with image_lib.LoopbackPartitions(FAKE_PATH) as lb:
      self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
      self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
      self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
      self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                         expected=False)
      self.assertEquals(lb.parts, LOOP_PARTS_DICT)
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def testManual(self):
    """Test using the loopback class closed manually."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEquals(lb.parts, LOOP_PARTS_DICT)
    lb.close()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def gcFunc(self):
    """This function isolates a local variable so it'll be garbage collected."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEquals(lb.parts, LOOP_PARTS_DICT)

  def testGarbageCollected(self):
    """Test using the loopback class closed by garbage collection."""
    self.gcFunc()
    # Force garbage collection in case python didn't already clean up the
    # loopback object.
    gc.collect()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])


class LsbUtilsTest(cros_test_lib.MockTempDirTestCase):
  """Tests the various LSB utilities."""

  def setUp(self):
    # Patch os.getuid(..) to pretend running as root, so reading/writing the
    # lsb-release file doesn't require escalated privileges and the test can
    # clean itself up correctly.
    self.PatchObject(os, 'getuid', return_value=0)

  def testWriteLsbRelease(self):
    """Tests writing out the lsb_release file using WriteLsbRelease(..)."""
    fields = {'x': '1', 'y': '2', 'foo': 'bar'}
    image_lib.WriteLsbRelease(self.tempdir, fields)
    lsb_release_file = os.path.join(self.tempdir, 'etc', 'lsb-release')
    expected_content = 'y=2\nx=1\nfoo=bar\n'
    self.assertFileContents(lsb_release_file, expected_content)

    # Test that WriteLsbRelease(..) correctly handles an existing file.
    fields = {'newkey1': 'value1', 'newkey2': 'value2', 'a': '3', 'b': '4'}
    image_lib.WriteLsbRelease(self.tempdir, fields)
    expected_content = ('y=2\nx=1\nfoo=bar\nnewkey2=value2\na=3\n'
                        'newkey1=value1\nb=4\n')
    self.assertFileContents(lsb_release_file, expected_content)


class BuildImagePathTest(cros_test_lib.MockTempDirTestCase):
  """BuildImagePath tests."""

  def setUp(self):
    self.board = 'board'
    self.board_dir = os.path.join(self.tempdir, self.board)

    D = cros_test_lib.Directory
    filesystem = (
        D(self.board, ('recovery_image.bin', 'other_image.bin')),
        'full_path_image.bin',
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    self.full_path = os.path.join(self.tempdir, 'full_path_image.bin')

  def testBuildImagePath(self):
    """BuildImagePath tests."""
    self.PatchObject(image_lib, 'GetLatestImageLink',
                     return_value=os.path.join(self.tempdir, self.board))

    # Board and full image path provided.
    result = image_lib.BuildImagePath(self.board, self.full_path)
    self.assertEqual(self.full_path, result)

    # Only full image path provided.
    result = image_lib.BuildImagePath(None, self.full_path)
    self.assertEqual(self.full_path, result)

    # Full image path provided that does not exist.
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(self.board, '/does/not/exist')
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(None, '/does/not/exist')

    # Default image is used.
    result = image_lib.BuildImagePath(self.board, None)
    self.assertEqual(os.path.join(self.board_dir, 'recovery_image.bin'), result)

    # Image basename provided.
    result = image_lib.BuildImagePath(self.board, 'other_image.bin')
    self.assertEqual(os.path.join(self.board_dir, 'other_image.bin'), result)

    # Image basename provided that does not exist.
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(self.board, 'does_not_exist.bin')

    default_mock = self.PatchObject(cros_build_lib, 'GetDefaultBoard')

    # Nothing provided, and no default.
    default_mock.return_value = None
    with self.assertRaises(image_lib.ImageDoesNotExistError):
      image_lib.BuildImagePath(None, None)

    # Nothing provided, with default.
    default_mock.return_value = 'board'
    result = image_lib.BuildImagePath(None, None)
    self.assertEqual(os.path.join(self.board_dir, 'recovery_image.bin'), result)


class SecurityTestConfigTest(cros_test_lib.RunCommandTempDirTestCase):
  """SecurityTestConfig class tests."""

  # pylint: disable=protected-access

  def setUp(self):
    self.image = '/path/to/image.bin'
    self.baselines = '/path/to/baselines'
    self.vboot_hash = 'abc123'
    self.config = image_lib.SecurityTestConfig(self.image, self.baselines,
                                               self.vboot_hash, self.tempdir)

  def testVbootCheckout(self):
    """Test normal flow - clone and checkout."""
    clone_patch = self.PatchObject(git, 'Clone')
    self.config._VbootCheckout()
    clone_patch.assert_called_once()
    self.assertCommandContains(['git', 'checkout', self.vboot_hash])

    # Make sure it doesn't try to clone & checkout again after already having
    # done so successfully.
    clone_patch = self.PatchObject(git, 'Clone')
    self.config._VbootCheckout()
    clone_patch.assert_not_called()

  def testVbootCheckoutError(self):
    """Test exceptions in a git command."""
    rce = cros_build_lib.RunCommandError('error', None)
    self.PatchObject(git, 'Clone', side_effect=rce)
    with self.assertRaises(image_lib.VbootCheckoutError):
      self.config._VbootCheckout()

  def testVbootCheckoutNoDirectory(self):
    """Test the error handling when the directory does not exist."""
    # Test directory that does not exist.
    self.config.directory = '/DOES/NOT/EXIST'
    with self.assertRaises(image_lib.SecurityConfigDirectoryError):
      self.config._VbootCheckout()

  def testRunCheck(self):
    """RunCheck tests."""
    # No config argument when running check.
    self.config.RunCheck('check1', False)
    check1 = os.path.join(self.config._checks_dir, 'ensure_check1.sh')
    config1 = os.path.join(self.baselines, 'ensure_check1.config')
    self.assertCommandContains([check1, self.image])
    self.assertCommandContains([config1], expected=False)

    # Include config argument when running check.
    self.config.RunCheck('check2', True)
    check2 = os.path.join(self.config._checks_dir, 'ensure_check2.sh')
    config2 = os.path.join(self.baselines, 'ensure_check2.config')
    self.assertCommandContains([check2, self.image, config2])

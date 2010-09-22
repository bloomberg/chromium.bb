#!/usr/bin/python
#
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for build_image shell script.

Note:
  This script must be run from INSIDE chroot.

Sample usage:
  # (inside chroot) pushd ~/trunk/src/scripts/
  # run all test cases in this script
  python chromite/tests/build_image_test.py

  # run all test cases in a test suite
  python chromite/tests/build_image_test.py BuildImageTest

  # run a specific test
  python chromite/tests/build_image_test.py BuildImageTest.testWithoutBoardExit
"""

import os
import re
import sys
import unittest
sys.path.append(os.path.join(os.path.dirname(__file__), '../lib'))
from cros_build_lib import (RunCommand, IsInsideChroot, GetChromeosVersion,
                            GetOutputImageDir)


class BuildImageTest(unittest.TestCase):
  """Test suite for build_image script."""

  def setUp(self):
    if not IsInsideChroot():
      raise RuntimeError('This script must be run from inside chroot.')

  def _CheckStringPresent(self, query_list, check_stdout=False):
    """Check for presence of specific queries.

    Args:
      query_list: a list of strings to look for.
      check_stdout: a boolean. True == use stdout from child process.
        Otherwise use its stderr.
    """
    for query in query_list:
      # Source error string defined in src/scripts/build_image
      if check_stdout:
        self.assertNotEqual(-1, self.output.find(query))
      else:
        self.assertNotEqual(-1, self.error.find(query))

  def _RunBuildImageCmd(self, cmd, assert_success=True):
    """Run build_image with flags.

    Args:
      cmd: a string.
      assert_success: a boolean. True == check child process return code is 0.
        False otherwise.
    """
    Info ('About to run command: %s' % cmd)
    cmd_result = RunCommand(
      cmd, error_ok=True, exit_code=True, redirect_stdout=True,
      redirect_stderr=True, shell=True)
    self.output = cmd_result.output
    self.error = cmd_result.error
    Info ('output =\n%r' % self.output)
    Info ('error =\n%r' % self.error)

    message = 'cmd should have failed! error:\n%s' % self.error
    if assert_success:
      self.assertEqual(0, cmd_result.returncode)
    else:
      self.assertNotEqual(0, cmd_result.returncode, message)

  def _VerifyOutputImagesExist(self, image_dir, image_list):
    """Verify output images exist in image_dir.

    Args:
      image_dir: a string, absolute path to output directory with images.
      image_list: a list of strings, names of output images.
    """
    for i in image_list:
      image_path = os.path.join(image_dir, i)
      self.assertTrue(os.path.exists(image_path))

  def testWithoutBoardExit(self):
    """Fail when no --board is specified."""
    self._RunBuildImageCmd('./build_image --board=""', assert_success=False)
    self._CheckStringPresent(['ERROR', '--board is required'])

  def testIncompatibleInstallFlags(self):
    """Fail when both --factory_install and --dev_install are set."""
    cmd = './build_image --board=x86-generic --factory_install --dev_install'
    self._RunBuildImageCmd(cmd, assert_success=False)
    self._CheckStringPresent(['ERROR', 'Incompatible flags'])

  def testIncompatibleRootfsFlags(self):
    """Fail when rootfs partition is not large enough."""
    cmd = ('./build_image --board=x86-generic --rootfs_size=100'
           ' --rootfs_hash_pad=10 --rootfs_partition_size=20')
    self._RunBuildImageCmd(cmd, assert_success=False)
    self._CheckStringPresent(['ERROR', 'bigger than partition'])

  def _BuildImageForBoard(self, board, image_list):
    """Build image for specific board type.

    Args:
      board: a string.
      image_list: a list of strings, names of output images.
    """
    cmd = './build_image --board=%s' % board
    Info ('If all goes well, it takes ~5 min. to build an image...')
    self._RunBuildImageCmd(cmd)
    self._CheckStringPresent(['Image created in', 'copy to USB keyfob'],
                             check_stdout=True)
    chromeos_version_str = GetChromeosVersion(self.output)
    image_dir = GetOutputImageDir(board, chromeos_version_str)
    self._VerifyOutputImagesExist(image_dir, image_list)

  def testBuildX86Generic(self):
    """Verify we can build an x86-generic image."""
    self._BuildImageForBoard(
      'x86-generic', ['chromiumos_image.bin', 'chromiumos_base_image.bin'])


if __name__ == '__main__':
  unittest.main()

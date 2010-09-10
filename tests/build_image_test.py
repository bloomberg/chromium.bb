#!/usr/bin/python
#
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for build_image.

Note:
  Some of the Shell scripts invoked in child process require sudo access.
  Be prepared to enter your password when prompted.

Sample usage:
  # run all test cases
  python build_image_test.py

  # run a specific test
  python build_image_test.py BuildImageTest.testBuildImageWithoutBoardExit
"""

import logging
import os
import re
import sys
import unittest
sys.path.append(os.path.join(os.path.dirname(__file__), '../lib'))
from cros_build_lib import RunCommand


logging.basicConfig(level=logging.INFO)


# TODO(tgao): move this into cros_build_lib.py
def IsInsideChroot():
  """Returns True if we are inside chroot."""
  return os.path.exists('/etc/debian_chroot')


def GetSrcRoot():
  """Get absolute path to src/scripts/ directory.

  Assuming test script will always be run from descendent of src/scripts.

  Returns:
    A string, absolute path to src/scripts directory. None if not found.
  """
  src_root = None
  match_str = '/src/scripts/'
  test_script_path = os.path.abspath('.')
  logging.debug('test_script_path = %r', test_script_path)

  path_list = re.split(match_str, test_script_path)
  logging.debug('path_list = %r', path_list)
  if path_list:
    src_root = os.path.join(path_list[0], match_str.strip('/'))
    logging.debug('src_root = %r', src_root)
  else:
    logging.debug('No %r found in %r', match_str, test_script_path)

  return src_root


# TODO(tgao): move this into cros_build_lib.py
def GetChromeosVersion(str_obj):
  """Helper method to parse output for CHROMEOS_VERSION_STRING.

  Args:
    str_obj: a string, which may contain Chrome OS version info.

  Returns:
    A string, value of CHROMEOS_VERSION_STRING environment variable set by
      chromeos_version.sh. Or None if not found.
  """
  match = re.search('CHROMEOS_VERSION_STRING=([0-9_.]+)', str_obj)
  if match and match.group(1):
    logging.info('CHROMEOS_VERSION_STRING = %s', match.group(1))
    return match.group(1)

  logging.info('CHROMEOS_VERSION_STRING NOT found')
  return None


# TODO(tgao): move this into cros_build_lib.py
def GetOutputImageDir(board, cros_version):
  """Construct absolute path to output image directory.

  Args:
    board: a string.
    cros_version: a string, Chrome OS version.

  Returns:
    a string: absolute path to output directory.
  """
  src_root = GetSrcRoot()
  rel_path = 'build/images/%s' % board
  # ASSUME: --build_attempt always sets to 1
  version_str = '-'.join([cros_version, 'a1'])
  output_dir = os.path.join(os.path.dirname(src_root), rel_path, version_str)
  logging.info('output_dir = %s', output_dir)
  return output_dir


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
    logging.info('About to run command: %s', cmd)
    cmd_result = RunCommand(
      cmd, error_ok=True, exit_code=True, redirect_stdout=True,
      redirect_stderr=True, shell=True)
    self.output = cmd_result.output
    self.error = cmd_result.error
    logging.info('output =\n%r', self.output)
    logging.info('error =\n%s', self.error)

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
    logging.info('If all goes well, it takes ~5 min. to build an image...')
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

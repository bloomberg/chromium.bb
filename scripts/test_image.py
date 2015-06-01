# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to mount a built image and run tests on it."""

from __future__ import print_function

import os
import unittest

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import image_test_lib
from chromite.lib import osutils
from chromite.lib import path_util


def ParseArgs(args):
  """Return parsed commandline arguments."""

  parser = commandline.ArgumentParser()
  parser.add_argument('--test_results_root', type='path',
                      help='Directory to store test results')
  parser.add_argument('--board', type=str, help='Board (wolf, beaglebone...)')
  parser.add_argument('image_dir', type='path',
                      help='Image directory (or file) with mount_image.sh and '
                           'umount_image.sh')
  opts = parser.parse_args(args)
  opts.Freeze()
  return opts


def FindImage(image_path):
  """Return the path to the image file.

  Args:
    image_path: A path to the image file, or a directory containing the base
      image.

  Returns:
    ImageFileAndMountScripts containing absolute paths to the image,
      the mount and umount invocation commands
  """

  if os.path.isdir(image_path):
    # Assume base image.
    image_file = os.path.join(image_path, constants.BASE_IMAGE_NAME + '.bin')
    if not os.path.exists(image_file):
      raise ValueError('Cannot find base image %s' % image_file)
  elif os.path.isfile(image_path):
    image_file = image_path
  else:
    raise ValueError('%s is neither a directory nor a file' % image_path)

  return image_file


def main(args):
  opts = ParseArgs(args)

  # Build up test suites.
  loader = unittest.TestLoader()
  loader.suiteClass = image_test_lib.ImageTestSuite
  # We use a different prefix here so that unittest DO NOT pick up the
  # image tests automatically because they depend on a proper environment.
  loader.testMethodPrefix = 'Test'
  all_tests = loader.loadTestsFromName('chromite.cros.test.image_test')
  forgiving = image_test_lib.ImageTestSuite()
  non_forgiving = image_test_lib.ImageTestSuite()
  for suite in all_tests:
    for test in suite.GetTests():
      if test.IsForgiving():
        forgiving.addTest(test)
      else:
        non_forgiving.addTest(test)

  # Run them in the image directory.
  runner = image_test_lib.ImageTestRunner()
  runner.SetBoard(opts.board)
  runner.SetResultDir(opts.test_results_root)
  image_file = FindImage(opts.image_dir)
  tmp_in_chroot = path_util.FromChrootPath('/tmp')
  with osutils.TempDir(base_dir=tmp_in_chroot) as temp_dir:
    with osutils.MountImageContext(image_file, temp_dir):
      with osutils.ChdirContext(temp_dir):
        # Run non-forgiving tests first so that exceptions in forgiving tests
        # do not skip any required tests.
        logging.info('Running NON-forgiving tests.')
        result = runner.run(non_forgiving)
        logging.info('Running forgiving tests.')
        runner.run(forgiving)

  if result and not result.wasSuccessful():
    return 1
  return 0

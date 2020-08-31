# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the security_test_image script."""

from __future__ import print_function

import os
import sys

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.scripts import security_test_image


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class SecurityTestImageTest(cros_test_lib.MockTempDirTestCase):
  """Security test image script tests."""

  def setUp(self):
    D = cros_test_lib.Directory
    filesystem = (
        D('board', ('recovery_image.bin',)),
        'other_image.bin',
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

  def testParseArgs(self):
    """Argument parsing tests."""
    # pylint: disable=protected-access
    # Test no arguments.
    self.PatchObject(cros_build_lib, 'GetDefaultBoard', return_value=None)
    with self.assertRaises(SystemExit):
      security_test_image._ParseArgs([])

    # Test board is set but not used when we have the full image path.
    self.PatchObject(cros_build_lib, 'GetDefaultBoard', return_value='board')
    opts = security_test_image._ParseArgs(
        ['--image', os.path.join(self.tempdir, 'other_image.bin')])
    self.assertEqual('board', opts.board)
    self.assertEqual(opts.image,
                     os.path.join(self.tempdir, 'other_image.bin'))

    # Test the board is fetched and used when using the default image basename.
    self.PatchObject(image_lib, 'GetLatestImageLink',
                     return_value=os.path.join(self.tempdir, 'board'))
    opts = security_test_image._ParseArgs([])
    self.assertEqual('board', opts.board)
    self.assertEqual(opts.image,
                     os.path.join(self.tempdir,
                                  'board/recovery_image.bin'))

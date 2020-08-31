# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test crows_setup_toolchains."""

from __future__ import print_function

import os
import sys

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cros_setup_toolchains


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class UtilsTest(cros_test_lib.MockTempDirTestCase):
  """Tests for various small util funcs."""

  def testFileIsCrosSdkElf(self):
    """Verify FileIsCrosSdkElf on x86_64 ELFs."""
    path = os.path.join(self.tempdir, 'file')
    data = (b'\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00'
            b'>\x00')
    osutils.WriteFile(path, data, mode='wb')
    self.assertTrue(cros_setup_toolchains.FileIsCrosSdkElf(path))

  def testArmIsNotCrosSdkElf(self):
    """Verify FileIsCrosSdkElf on aarch64 ELFs."""
    path = os.path.join(self.tempdir, 'file')
    data = (b'\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00'
            b'(\x00')
    osutils.WriteFile(path, data, mode='wb')
    self.assertFalse(cros_setup_toolchains.FileIsCrosSdkElf(path))

  def testScriptIsNotCrosSdkElf(self):
    """Verify FileIsCrosSdkElf on shell scripts."""
    path = os.path.join(self.tempdir, 'file')
    data = '#!/bin/sh\necho hi\n'
    osutils.WriteFile(path, data)
    self.assertFalse(cros_setup_toolchains.FileIsCrosSdkElf(path))

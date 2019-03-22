# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Signing tests."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import signing


class GetDefaultVbootStableHashTest(cros_test_lib.TempDirTestCase):
  """GetDefaultVbootStableHash tests."""

  def setUp(self):
    D = cros_test_lib.Directory
    filesystem = (
        D('configs', ('cros_common.config',)),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    self.config_file = os.path.join(self.tempdir, 'configs/cros_common.config')
    self.hash = 'abc123'
    content = '[signer]\nvboot_stable_hash = %s' % self.hash
    osutils.WriteFile(self.config_file, content)

  def testValidConfigRead(self):
    """Test successful read from valid file."""
    result = signing.GetDefaultVbootStableHash(config_file=self.config_file)
    self.assertEqual(self.hash, result)

  def testInvalidConfigRead(self):
    """Test reading non-existent file and no option."""
    # No file.
    self.assertIsNone(signing.GetDefaultVbootStableHash(
        config_file=os.path.join(self.tempdir, 'DOES_NOT_EXIST')))

    # Section exists but not the option.
    osutils.WriteFile(self.config_file, '[signer]')
    hash1 = signing.GetDefaultVbootStableHash(config_file=self.config_file)
    self.assertIsNone(hash1)

    # No section or option.
    osutils.WriteFile(self.config_file, '')
    hash2 = signing.GetDefaultVbootStableHash(config_file=self.config_file)
    self.assertIsNone(hash2)

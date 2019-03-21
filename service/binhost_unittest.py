# -*- coding: utf-8 -*-path + os.sep)
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the binhost.py service."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.service import binhost

class BinhostTest(cros_test_lib.MockTempDirTestCase):
  """Unit test definitions."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)

    self.public_conf_dir = os.path.join(
        self.tempdir, constants.PUBLIC_BINHOST_CONF_DIR, 'target')
    osutils.SafeMakedirs(self.public_conf_dir)

    self.private_conf_dir = os.path.join(
        self.tempdir, constants.PRIVATE_BINHOST_CONF_DIR, 'target')
    osutils.SafeMakedirs(self.private_conf_dir)

  def tearDown(self):
    osutils.EmptyDir(self.tempdir)

  def testSetBinhostPublic(self):
    """SetBinhost returns correct public path and updates conf file."""
    actual = binhost.SetBinhost(
        'coral', 'BINHOST_KEY', 'gs://prebuilts', private=False)
    expected = os.path.join(self.public_conf_dir, 'coral-BINHOST_KEY.conf')
    self.assertEqual(actual, expected)
    self.assertEqual(osutils.ReadFile(actual), 'BINHOST_KEY="gs://prebuilts"')

  def testSetBinhostPrivate(self):
    """SetBinhost returns correct private path and updates conf file."""
    actual = binhost.SetBinhost('coral', 'BINHOST_KEY', 'gs://prebuilts')
    expected = os.path.join(self.private_conf_dir, 'coral-BINHOST_KEY.conf')
    self.assertEqual(actual, expected)
    self.assertEqual(osutils.ReadFile(actual), 'BINHOST_KEY="gs://prebuilts"')

  def testSetBinhostEmptyConf(self):
    """SetBinhost rejects existing but empty conf files."""
    conf_path = os.path.join(self.private_conf_dir, 'multi-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, ' ')
    with self.assertRaises(ValueError):
      binhost.SetBinhost('multi', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhostMultilineConf(self):
    """SetBinhost rejects existing multiline conf files."""
    conf_path = os.path.join(self.private_conf_dir, 'multi-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, '\n'.join(['A="foo"', 'B="bar"']))
    with self.assertRaises(ValueError):
      binhost.SetBinhost('multi', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhhostBadConfLine(self):
    """SetBinhost rejects existing conf files with malformed lines."""
    conf_path = os.path.join(self.private_conf_dir, 'bad-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, 'bad line')
    with self.assertRaises(ValueError):
      binhost.SetBinhost('bad', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhostMismatchedKey(self):
    """SetBinhost rejects existing conf files with a mismatched key."""
    conf_path = os.path.join(self.private_conf_dir, 'bad-key-GOOD_KEY.conf')
    osutils.WriteFile(conf_path, 'BAD_KEY="https://foo.bar"')
    with self.assertRaises(KeyError):
      binhost.SetBinhost('bad-key', 'GOOD_KEY', 'gs://blah')

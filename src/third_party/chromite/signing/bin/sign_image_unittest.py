# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS sign_image unittests"""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.signing.image_signing import imagefile
from chromite.signing.bin import sign_image


class TestMain(cros_test_lib.RunCommandTestCase):
  """Test sign_image."""

  def setUp(self):
    self.rc.SetDefaultCmdResult()
    self.infile = '/path/to/image.bin'
    self.outfile = '/path/to/output.bin'
    self.sign_mock = self.PatchObject(imagefile, 'SignImage')

  def testSsd(self):
    """Signs image_type=SSD."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'ssd', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'SSD', self.infile, self.outfile, 2, '/keydir', '')

  def testBase(self):
    """Signs image_type=base."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'base', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'SSD', self.infile, self.outfile, 2, '/keydir', '')

  def testUsb(self):
    """Signs image_type=usb."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'usb', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'USB', self.infile, self.outfile, 2, '/keydir', '')

  def testRecovery(self):
    """Signs image_type=recovery."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'recovery', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'recovery', self.infile, self.outfile, 4, '/keydir', 'recovery_')

  def testFactory(self):
    """Signs image_type=factory."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'factory', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'factory_install', self.infile, self.outfile, 2, '/keydir',
        'installer_')

  def testInstall(self):
    """Signs image_type=install."""
    self.assertEqual(0, sign_image.main(
        ['--type', 'install', '-i', self.infile, '--keyset-dir', '/keydir',
         '-o', self.outfile]))
    self.sign_mock.assert_called_once_with(
        'factory_install', self.infile, self.outfile, 2, '/keydir',
        'installer_')

  def testBadType(self):
    """Fails with bad image_type."""
    with self.assertRaises(SystemExit):
      sign_image.main(['--type', 'badtype', '-i', self.infile,
                       '--keyset-dir', '/keydir', '-o', self.outfile])
    self.assertEqual(0, self.sign_mock.call_count)

  def testRejectsVersionFile(self):
    """Rejects obsolete --version-file."""
    with self.assertRaises(SystemExit):
      sign_image.main(['--type', 'badtype', '-i', self.infile,
                       '--keyset-dir', '/keydir', '-o', self.outfile,
                       '--version-file', 'key.versions'])
    self.assertEqual(0, self.sign_mock.call_count)

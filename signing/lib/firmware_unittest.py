# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests Firmware related signing"""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.signing.lib import firmware
from chromite.signing.lib import keys
from chromite.signing.lib import keys_unittest
from chromite.signing.lib import signer_unittest


class TestBiosSigner(cros_test_lib.RunCommandTempDirTestCase):
  """Test BiosSigner."""

  def testGetCmdArgs(self):
    bs = firmware.BiosSigner()
    ks = signer_unittest.KeysetFromSigner(bs, self.tempdir)

    bios_bin = os.path.join(self.tempdir, 'bios.bin')
    bios_out = os.path.join(self.tempdir, 'bios.out')

    fw_key = ks.keys['firmware_data_key']
    kernel_key = ks.keys['kernel_subkey']
    self.assertListEqual(bs.GetFutilityArgs(ks, bios_bin, bios_out),
                         ['sign',
                          '--type', 'bios',
                          '--signprivate', fw_key.private,
                          '--keyblock', fw_key.keyblock,
                          '--kernelkey', kernel_key.public,
                          '--version', fw_key.version,
                          '--devsign', fw_key.private,
                          '--devkeyblock', fw_key.keyblock,
                          bios_bin, bios_out])

  def testGetCmdArgsWithDevKeys(self):
    bs = firmware.BiosSigner()
    ks = signer_unittest.KeysetFromSigner(bs, self.tempdir)

    bios_bin = os.path.join(self.tempdir, 'bios.bin')
    bios_out = os.path.join(self.tempdir, 'bios.out')

    # Add 'dev_firmware' keys and keyblock
    dev_fw_key = keys.KeyPair('dev_firmware_data_key', keydir=self.tempdir)
    ks.AddKey(dev_fw_key)
    keys_unittest.CreateDummyPrivateKey(dev_fw_key)
    keys_unittest.CreateDummyKeyblock(dev_fw_key)

    fw_key = ks.keys['firmware_data_key']
    kernel_key = ks.keys['kernel_subkey']

    self.assertListEqual(bs.GetFutilityArgs(ks, bios_bin, bios_out),
                         ['sign',
                          '--type', 'bios',
                          '--signprivate', fw_key.private,
                          '--keyblock', fw_key.keyblock,
                          '--kernelkey', kernel_key.public,
                          '--version', fw_key.version,
                          '--devsign', dev_fw_key.private,
                          '--devkeyblock', dev_fw_key.keyblock,
                          bios_bin, bios_out])

  def testGetCmdArgsWithSig(self):
    loem_dir = os.path.join(self.tempdir, 'loem1', 'keyset')
    loem_id = 'loem1'

    bs = firmware.BiosSigner(sig_id=loem_id, sig_dir=loem_dir)
    ks = signer_unittest.KeysetFromSigner(bs, keydir=self.tempdir)

    bios_bin = os.path.join(self.tempdir, loem_id, 'bios.bin')

    fw_key = ks.keys['firmware_data_key']
    kernel_key = ks.keys['kernel_subkey']

    self.assertListEqual(bs.GetFutilityArgs(ks, bios_bin, loem_dir),
                         ['sign',
                          '--type', 'bios',
                          '--signprivate', fw_key.private,
                          '--keyblock', fw_key.keyblock,
                          '--kernelkey', kernel_key.public,
                          '--version', fw_key.version,
                          '--devsign', fw_key.private,
                          '--devkeyblock', fw_key.keyblock,
                          '--loemdir', loem_dir,
                          '--loemid', loem_id,
                          bios_bin, loem_dir])


class TestECSigner(cros_test_lib.RunCommandTempDirTestCase):
  """Test ECSigner."""

  def testIsROSignedRW(self):
    ec_signer = firmware.ECSigner()
    bios_bin = os.path.join(self.tempdir, 'bin.bin')

    self.assertFalse(ec_signer.IsROSigned(bios_bin))

    self.assertCommandContains(['futility', 'dump_fmap', bios_bin])

  def testIsROSignedRO(self):
    ec_signer = firmware.ECSigner()
    bios_bin = os.path.join(self.tempdir, 'bin.bin')

    self.rc.SetDefaultCmdResult(output='KEY_RO')

    self.assertTrue(ec_signer.IsROSigned(bios_bin))

  def testSign(self):
    ec_signer = firmware.ECSigner()
    ks = signer_unittest.KeysetFromSigner(ec_signer, self.tempdir)
    ec_bin = os.path.join(self.tempdir, 'ec.bin')
    bios_bin = os.path.join(self.tempdir, 'bios.bin')

    self.assertTrue(ec_signer.Sign(ks, ec_bin, bios_bin))

    self.assertCommandContains(['futility', 'sign', '--type', 'rwsig',
                                '--prikey', ks.keys['key_ec_efs'].private,
                                ec_bin])
    self.assertCommandContains(['openssl', 'dgst', '-sha256', '-binary'])
    self.assertCommandContains(['store_file_in_cbfs', bios_bin, 'ecrw'])
    self.assertCommandContains(['store_file_in_cbfs', bios_bin, 'ecrw.hash'])


class TestGBBSigner(cros_test_lib.TempDirTestCase):
  """Test GBBSigner."""

  def testGetFutilityArgs(self):
    gb_signer = firmware.GBBSigner()
    ks = signer_unittest.KeysetFromSigner(gb_signer, self.tempdir)

    bios_in = os.path.join(self.tempdir, 'bin.orig')
    bios_out = os.path.join(self.tempdir, 'bios.bin')

    self.assertListEqual(gb_signer.GetFutilityArgs(ks, bios_in, bios_out),
                         ['gbb',
                          '--set',
                          '--recoverykey=' + ks.keys['recovery_key'].public,
                          bios_in,
                          bios_out])


class ShellballTest(cros_test_lib.RunCommandTempDirTestCase):
  """Verify that shellball is being called with correct arguments."""

  def setUp(self):
    """Setup simple Shellball instance for mock testing."""
    self.sb1name = os.path.join(self.tempdir, 'fooball')
    osutils.Touch(self.sb1name)
    self.sb1 = firmware.Shellball(self.sb1name)

  def testExtractCall(self):
    """Test arguments for image extract."""
    out_dir = 'bar'
    expected_args = ['--sb_extract', out_dir]

    self.sb1.Extract(out_dir)
    self.assertCommandContains(expected_args)

  def testRepackCall(self):
    """Test arguments for image repack."""
    from_dir = 'bar'
    expected_args = ['--sb_repack', from_dir]

    self.sb1.Repack(from_dir)
    self.assertCommandContains(expected_args)

  def testContextManager(self):
    with self.sb1 as sb_dir:
      self.assertExists(sb_dir)
      self.assertCommandContains(['--sb_extract'])
    self.assertCommandContains(['--sb_repack'])

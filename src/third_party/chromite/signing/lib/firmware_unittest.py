# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests Firmware related signing"""

from __future__ import print_function

import csv
import io
import os
import shutil
import textwrap

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.signing.lib import firmware
from chromite.signing.lib import keys
from chromite.signing.lib import keys_unittest
from chromite.signing.lib import signer_unittest


def MockDumpFmap(rc, ec_ro=True):
  """Add futility dump_fmap mock for bios.bin and ec.bin."""
  bios_output = textwrap.dedent('''
    SI_ALL 0 2097152
    SI_DESC 0 4096
    SI_ME 4096 2093056
    SI_BIOS 2097152 14680064
    RW_SECTION_A 2097152 4096000
    VBLOCK_A 2097152 65536
    FW_MAIN_A 2162688 4030400
    RW_FWID_A 6193088 64
    RW_SECTION_B 6193152 4096000
    VBLOCK_B 6193152 65536
    FW_MAIN_B 6258688 4030400
    RW_FWID_B 10289088 64
    RW_MISC 10289152 196608
    UNIFIED_MRC_CACHE 10289152 131072
    RECOVERY_MRC_CACHE 10289152 65536
    RW_MRC_CACHE 10354688 65536
    RW_ELOG 10420224 16384
    RW_SHARED 10436608 16384
    SHARED_DATA 10436608 8192
    VBLOCK_DEV 10444800 8192
    RW_VPD 10452992 8192
    RW_NVRAM 10461184 24576
    RW_LEGACY 10485760 2097152
    WP_RO 12582912 4194304
    RO_VPD 12582912 16384
    RO_UNUSED 12599296 49152
    RO_SECTION 12648448 4128768
    FMAP 12648448 2048
    RO_FRID 12650496 64
    RO_FRID_PAD 12650560 1984
    GBB 12652544 978944
    COREBOOT 13631488
    ''')

  ec_output = textwrap.dedent('''
    EC_RO 64 131072
    FR_MAIN 64 131072
    RO_FRID 388 32
    FMAP 104000 518
    WP_RO 0 262144
    EC_RW 262144 131072
    RW_FWID 262468 32
    SIG_RW 392192 1024
    EC_RW_B 393216 131072
    ''')
  if ec_ro:
    ec_output += 'KEY_RO 130112 1024'

  rc.AddCmdResult(partial_mock.ListRegex('futility dump_fmap -p .*bios.bin'),
                  output=bios_output)

  rc.AddCmdResult(partial_mock.ListRegex('futility dump_fmap -p .*ec.bin'),
                  output=ec_output)

def MockBiosSigner(rc):
  """Add Bios Signing Mocks to |rc|."""

  def _copy_firmware(cmd, *_args, **_kwargs):
    """Copy file_in to file_out, if file_in exists."""
    file_in = cmd[-2]
    file_out = cmd[-1]
    if os.path.exists(file_in):
      shutil.copy(file_in, file_out)

  rc.AddCmdResult(partial_mock.ListRegex('futility sign --type bios .*'),
                  side_effect=_copy_firmware)

def MockECSigner(rc, ec_ro=True):
  """Add EC Signing Mocks to |rc|.

  Args:
    rc: RunCommandMock that cmds are added to
    ec_ro: Treat EC as RO in fmap
  """
  rc.AddCmdResult(partial_mock.ListRegex('futility sign --type rwsig .*'))
  rc.AddCmdResult(partial_mock.ListRegex('openssl dgst -sha256 -binary .*'))
  rc.AddCmdResult(partial_mock.ListRegex('store_file_in_cbfs .*'))

  MockDumpFmap(rc, ec_ro=ec_ro)

def MockGBBSigner(rc):
  """Add GBB Signer Mock commands to |rc|"""
  rc.AddCmdResult(partial_mock.ListRegex('futility gbb'))

def MockFirmwareSigner(rc):
  """Add mocks to |rc| for Firmware signing."""
  keys_unittest.MockVbutilKey(rc)
  MockBiosSigner(rc)
  MockECSigner(rc, ec_ro=False)
  MockGBBSigner(rc)


class TestBiosSigner(cros_test_lib.RunCommandTempDirTestCase):
  """Test BiosSigner."""

  def setUp(self):
    MockBiosSigner(self.rc)

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

  def testGetCmdArgsWithPreamble(self):
    bs = firmware.BiosSigner(preamble_flags=1)
    ks = signer_unittest.KeysetFromSigner(bs, self.tempdir)

    bios_bin = os.path.join(self.tempdir, 'bios.bin')
    bios_out = os.path.join(self.tempdir, 'bios.out')

    # Add 'dev_firmware' keys and keyblock
    dev_fw_key = keys.KeyPair('dev_firmware_data_key', keydir=self.tempdir)
    ks.AddKey(dev_fw_key)
    keys_unittest.CreateDummyPrivateKey(dev_fw_key)
    keys_unittest.CreateDummyKeyblock(dev_fw_key)

    args = bs.GetFutilityArgs(ks, bios_bin, bios_out)

    self.assertIn('--flags', args)
    self.assertEqual(args[args.index('--flags') + 1], '1')

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
    MockECSigner(self.rc, ec_ro=False)
    ec_signer = firmware.ECSigner()
    ec_bin = os.path.join(self.tempdir, 'ec.bin')

    self.assertFalse(ec_signer.IsROSigned(ec_bin))

    self.assertCommandContains(['futility', 'dump_fmap', '-p', ec_bin])

  def testIsROSignedRO(self):
    MockECSigner(self.rc)
    ec_signer = firmware.ECSigner()
    ec_bin = os.path.join(self.tempdir, 'ec.bin')

    self.assertTrue(ec_signer.IsROSigned(ec_bin))

  def testSign(self):
    MockECSigner(self.rc, ec_ro=False)
    ec_signer = firmware.ECSigner()
    ks = signer_unittest.KeysetFromSigner(ec_signer, self.tempdir)
    ec_bin = os.path.join(self.tempdir, 'ec.bin')
    bios_bin = os.path.join(self.tempdir, 'bios.bin')

    ec_signer.Sign(ks, ec_bin, bios_bin)

    self.assertCommandContains(['futility', 'sign', '--type', 'rwsig',
                                '--prikey', ks.keys['key_ec_efs'].private,
                                ec_bin])
    self.assertCommandContains(['openssl', 'dgst', '-sha256', '-binary'])
    self.assertCommandContains(['store_file_in_cbfs', bios_bin, 'ecrw'])
    self.assertCommandContains(['store_file_in_cbfs', bios_bin, 'ecrw.hash'])


class TestFirmwareSigner(cros_test_lib.RunCommandTempDirTestCase):
  """Test FirmwareSigner."""

  def setUp(self):
    MockFirmwareSigner(self.rc)

  def testSignOneSimple(self):
    fs = firmware.FirmwareSigner()
    ks = signer_unittest.KeysetFromSigner(fs, self.tempdir)

    shellball_dir = os.path.join(self.tempdir, 'shellball')
    bios_path = os.path.join(shellball_dir, 'bios.bin')

    fs.SignOne(ks, self.tempdir, bios_path)

    self.assertCommandContains(['futility', 'sign', '--type', 'bios'])
    self.assertCommandContains(['futility', 'gbb'])

  def testSignOneWithEC(self):
    fs = firmware.FirmwareSigner()
    ks = keys_unittest.KeysetMock(os.path.join(self.tempdir, 'keyset'))
    ks.CreateDummyKeys()

    shellball_dir = os.path.join(self.tempdir, 'shellball')
    bios_path = os.path.join(shellball_dir, 'bios.bin')
    ec_path = os.path.join(shellball_dir, 'ec.bin')

    fs.SignOne(ks, self.tempdir, bios_path, ec_image=ec_path)

    self.assertCommandContains(['futility', 'sign', '--type', 'rwsig', ec_path])

  def testSignOneWithLoem(self):
    fs = firmware.FirmwareSigner()
    ks = keys_unittest.KeysetMock(os.path.join(self.tempdir, 'keyset'))
    ks.CreateDummyKeys()
    ks_subset = ks.GetSubKeyset('ACME')

    shellball_dir = os.path.join(self.tempdir, 'shellball')
    bios_path = os.path.join(shellball_dir, 'bios.bin')

    fs.SignOne(ks, shellball_dir, bios_path, model_name='acme-board',
               key_id='ACME')

    self.assertExists(os.path.join(shellball_dir, 'rootkey.acme-board'))

    self.assertCommandContains(['futility', 'sign', '--type', 'bios',
                                '--signprivate',
                                ks_subset.keys['firmware_data_key'].private])

  def testSignWithSignerConfig(self):
    fs = firmware.FirmwareSigner()
    ks = keys_unittest.KeysetMock(os.path.join(self.tempdir, 'keyset'))
    ks.CreateDummyKeys()

    shellball_dir = os.path.join(self.tempdir, 'shellball')
    osutils.SafeMakedirs(shellball_dir)

    # Create signer_config.csv
    signer_config_csv = os.path.join(shellball_dir, 'signer_config.csv')
    with open(signer_config_csv, 'w') as csv_file:
      csv_file.write(SignerConfigsFromCSVTest.CreateCSV().read())

    fs.Sign(ks, shellball_dir, None)

    for board_config in SignerConfigsFromCSVTest.CreateBoardDicts():
      bios_path = os.path.join(shellball_dir, board_config['firmware_image'])
      self.assertCommandContains(['futility', 'sign', bios_path])

  def testSignWithNoSignerConfig(self):
    fs = firmware.FirmwareSigner()
    ks = keys_unittest.KeysetMock(os.path.join(self.tempdir, 'keyset'))
    ks.CreateDummyKeys()

    shellball_dir = os.path.join(self.tempdir, 'shellball')

    test_bios = ('bios.bin',
                 'bios.loem1.bin')

    for bios in test_bios:
      osutils.Touch(os.path.join(shellball_dir, bios), makedirs=True)

    fs.Sign(ks, shellball_dir, None)

    for bios in test_bios:
      bios_path = os.path.join(shellball_dir, bios)
      self.assertCommandContains(['futility', 'sign', bios_path])

    self.assertExists(os.path.join(shellball_dir, 'keyset.loem1'))


class TestGBBSigner(cros_test_lib.RunCommandTempDirTestCase):
  """Test GBBSigner."""

  def setUp(self):
    MockGBBSigner(self.rc)

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


  @staticmethod
  def CmdMock(rc):
    """Add mock commands to |rc|"""
    rc.AddCmdResult(partial_mock.ListRegex('.* --sb_extract .*'))
    rc.AddCmdResult(partial_mock.ListRegex('.* --sb_repack .*'))


  def setUp(self):
    """Setup simple Shellball instance for mock testing."""
    ShellballTest.CmdMock(self.rc)
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


class SignerConfigsFromCSVTest(cros_test_lib.TestCase):
  """Test SignerConfigsFromCSV function."""

  FIELDS = ('model_name', 'firmware_image', 'key_id', 'ec_image')
  BOARDS = (('acme', 'models/acme/bios.bin', 'ACME', 'models/acme/ec.bin'),
            ('shinra', 'models/shinra/bios.bin', 'SHINRA',
             'models/shinra/ec.bin'),
            ('acme-loem3', 'models/acme/bios.bin', 'loem3',
             'models/acme/ec.bin'))

  @staticmethod
  def CreateBoardDicts(fields=None, boards=None):
    """Returns list of board dicts with the given fields"""
    fields = fields or SignerConfigsFromCSVTest.FIELDS
    boards = boards or SignerConfigsFromCSVTest.BOARDS

    board_dicts = []

    for board in boards:
      board_dicts.append(dict(zip(fields, board)))

    return board_dicts

  @staticmethod
  def CreateCSV(fields=None, boards=None, board_dict=None):
    fields = fields or SignerConfigsFromCSVTest.FIELDS
    boards = boards or SignerConfigsFromCSVTest.BOARDS

    board_dicts = (board_dict or
                   SignerConfigsFromCSVTest.CreateBoardDicts(fields=fields,
                                                             boards=boards))

    csv_file = io.BytesIO()
    csv_writer = csv.DictWriter(csv_file, fields)

    csv_writer.writeheader()
    csv_writer.writerows(board_dicts)

    return io.BytesIO(csv_file.getvalue())

  def testMissingRow(self):
    fields = SignerConfigsFromCSVTest.BOARDS[:-1]
    csv_file = SignerConfigsFromCSVTest.CreateCSV(fields=fields)

    with self.assertRaises(csv.Error):
      firmware.SignerConfigsFromCSV(csv_file)

  def testSimple(self):
    orig_boards = SignerConfigsFromCSVTest.CreateBoardDicts()
    csv_file = SignerConfigsFromCSVTest.CreateCSV(board_dict=orig_boards)

    signer_configs = firmware.SignerConfigsFromCSV(csv_file)

    self.assertListEqual(orig_boards, signer_configs)


class TestWriteSignerNotes(cros_test_lib.RunCommandTempDirTestCase):
  """Test WriteSignerNotes function."""

  def setUp(self):
    keys_unittest.MockVbutilKey(self.rc)

  def testSingleKey(self):
    """Test function's output with fixed sha1sum."""
    recovery_key = keys.KeyPair('recovery_key', self.tempdir)
    root_key = keys.KeyPair('root_key', self.tempdir)

    keyset = keys.Keyset()
    keyset.AddKey(recovery_key)
    keyset.AddKey(root_key)


    sha1sum = keys_unittest.MOCK_SHA1SUM
    expected_output = ['Signed with keyset in ' + self.tempdir,
                       'recovery: ' + sha1sum,
                       'root: ' + sha1sum]

    version_signer = io.BytesIO()
    firmware.WriteSignerNotes(keyset, version_signer)
    self.assertListEqual(expected_output,
                         version_signer.getvalue().splitlines())

  def testLoemKeys(self):
    """Test function's output with multiple loem keys."""
    recovery_key = keys.KeyPair('recovery_key', self.tempdir)
    root_key = keys.KeyPair('root_key', self.tempdir)
    root_key.AddSubkey('loem1')
    root_key.AddSubkey('loem2')
    root_key.AddSubkey('loem3')

    keyset = keys.Keyset()
    keyset.AddKey(recovery_key)
    keyset.AddKey(root_key)

    sha1sum = keys_unittest.MOCK_SHA1SUM
    expected_header = ['Signed with keyset in ' + self.tempdir,
                       'recovery: ' + sha1sum,
                       'List sha1sum of all loem/model\'s signatures:']

    expected_loems = ['loem1: ' + sha1sum,
                      'loem2: ' + sha1sum,
                      'loem3: ' + sha1sum]

    version_signer = io.BytesIO()
    firmware.WriteSignerNotes(keyset, version_signer)

    output = version_signer.getvalue().splitlines()

    output_header = output[:len(expected_header)]
    output_loem = output[len(expected_header):]

    output_loem.sort() # loem's can be out of order, so sort first.

    self.assertListEqual(expected_header, output_header)
    self.assertListEqual(expected_loems, output_loem)

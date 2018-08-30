# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""""Unittest ChromeOS image signer logic"""

from __future__ import print_function

import ConfigParser
import io

from chromite.lib import cros_test_lib
from chromite.signing.lib import keys
from chromite.signing.lib import signer
from chromite.signing.lib import keys_unittest


class TestSignerConfig(cros_test_lib.TestCase):
  """Test SignerConfig."""

  def GetSignerConfig(self, archive='foo.tar.bz2', board='link',
                      artifact_type='update_payload', version='1.2.3.4',
                      versionrev='R24-1.2.3.4', keyset='link-mp', channel='dev',
                      input_files=('foo.bin'),
                      output_files=('@ROOTNAME@-@VERSION@.bin')):
    """"Returns SignerConfig, providing sane defaults."""
    return(signer.SignerInstructionConfig(archive=archive,
                                          board=board,
                                          artifact_type=artifact_type,
                                          version=version,
                                          versionrev=versionrev,
                                          keyset=keyset,
                                          channel=channel,
                                          input_files=input_files,
                                          output_files=output_files))

  def testToIniDictSimple(self):
    self.assertDictEqual(self.GetSignerConfig().ToIniDict(),
                         {'general':{'archive':'foo.tar.bz2',
                                     'board':'link',
                                     'type':'update_payload',
                                     'version':'1.2.3.4',
                                     'versionrev':'R24-1.2.3.4'},
                          'insns':{'keyset':'link-mp',
                                   'channel':'dev',
                                   'input_files':'foo.bin',
                                   'output_names':'@ROOTNAME@-@VERSION@.bin'}
                         })

  def testReadIniFile(self):
    initial_sc = self.GetSignerConfig()

    # Create INI file from initial SignerConfig
    cp = ConfigParser.ConfigParser()
    for section, options in initial_sc.ToIniDict().items():
      cp.add_section(section)
      for option, value in options.items():
        cp.set(section, option, value=value)
    ini_in_file = io.BytesIO()
    cp.write(ini_in_file)

    # Read INI to new SignerConfig
    read_sc = signer.SignerInstructionConfig()
    read_sc.ReadIniFile(io.BytesIO(ini_in_file.getvalue()))

    self.assertEqual(initial_sc, read_sc)

  def testGetFilePairsSimple(self):
    in_files = 'foo.bar'
    out_files = 'foo.out.bar'
    sc = self.GetSignerConfig(input_files=in_files, output_files=out_files)
    self.assertListEqual(sc.GetFilePairs(), [(in_files, out_files)])

  def testGetFilePairsSimpleMultiple(self):
    in_files = ('foo.bin', 'bar.bin')
    out_files = ('foo.out.bin', 'bar.out.bin')
    sc = self.GetSignerConfig(input_files=in_files, output_files=out_files)
    self.assertListEqual(sc.GetFilePairs(), [('foo.bin', 'foo.out.bin'),
                                             ('bar.bin', 'bar.out.bin')])

  def testGetFilePairsSimpleTemplate(self):
    in_files = 'foo.bar'
    sc = self.GetSignerConfig(input_files=in_files)
    self.assertListEqual(sc.GetFilePairs(), [(in_files, 'foo-1.2.3.4.bin')])

  def testGetFilePairsDefault(self):
    in_file = 'foo.bar'
    out_file = 'chromeos_1.2.3.4_link_update_payload_dev-channel_link-mp.bin'
    sc = self.GetSignerConfig(input_files=in_file, output_files=())
    self.assertListEqual(sc.GetFilePairs(), [(in_file, out_file)])

  def testGetFilePairsMultipleInput(self):
    in_files = ('foo.bin', 'bar.bin')
    sc = self.GetSignerConfig(input_files=in_files)
    self.assertListEqual(sc.GetFilePairs(), [('foo.bin', 'foo-1.2.3.4.bin'),
                                             ('bar.bin', 'bar-1.2.3.4.bin')])

  def testGetFilePairsMultipleInputDefaultTemp(self):
    in_files = ('foo.bin', 'bar.bin')
    sc = self.GetSignerConfig(input_files=in_files, output_files=())
    with self.assertRaises(signer.SignerOutputTemplateError):
      sc.GetFilePairs()

  def testFillTemplate(self):
    sc = self.GetSignerConfig()

    in_file = '/tmp/foo.bar'
    self.assertEqual('foo.out', sc.FillTemplate('foo.out'))
    self.assertEqual('foo.out', sc.FillTemplate('foo.out', filename=in_file))

    self.assertEqual('__link__', sc.FillTemplate('__@BOARD@__'))
    self.assertEqual('__dev__', sc.FillTemplate('__@CHANNEL@__'))
    self.assertEqual('__link-mp__', sc.FillTemplate('__@KEYSET@__'))
    self.assertEqual('__update_payload__', sc.FillTemplate('__@TYPE@__'))
    self.assertEqual('__1.2.3.4__', sc.FillTemplate('__@VERSION@__'))

    self.assertEqual('__foo.bar__',
                     sc.FillTemplate('__@BASENAME@__', filename=in_file))

    self.assertEqual('__foo__',
                     sc.FillTemplate('__@ROOTNAME@__', filename=in_file))


class MockBaseSigner(signer.BaseSigner):
  """Configurable Signer for testing."""
  def __init__(self, required_keys=None,
               required_keys_public=None,
               required_keys_private=None,
               required_keyblocks=None):
    """Create a Signer based on the passed required lists."""
    self._required_keys = required_keys or []
    self._required_keys_public = required_keys_public or []
    self._required_keys_private = required_keys_private or []
    self._required_keyblocks = required_keyblocks or []

  def Sign(self, keyset, input_name, output_name):
    """Always return True on signing."""
    return True


class TestSigner(cros_test_lib.TempDirTestCase):
  """Test Signer."""

  def testSign(self):
    ks = keys.Keyset()
    s = signer.BaseSigner()
    with self.assertRaises(NotImplementedError):
      s.Sign(ks, 'input', 'output')

  def testCheck(self):
    ks = keys.Keyset()
    s = signer.BaseSigner()
    self.assertTrue(s.CheckKeyset(ks))

  def testCheckRequiredKeysMissing(self):
    ks_empty = keys.Keyset()
    s0 = MockBaseSigner(required_keys=['key1'])
    self.assertFalse(s0.CheckKeyset(ks_empty))

  def testCheckRequiredKeys(self):
    s0 = MockBaseSigner(required_keys=['key1'])
    ks0 = KeysetFromSigner(s0, self.tempdir)
    self.assertTrue(s0.CheckKeyset(ks0))

  def testCheckRequiredPublicKeysMissing(self):
    ks_empty = keys.Keyset()
    s0 = MockBaseSigner(required_keys_public=['key1'])
    self.assertFalse(s0.CheckKeyset(ks_empty))

  def testCheckRequiredPublicKeys(self):
    s0 = MockBaseSigner(required_keys_public=['key1'])
    ks0 = KeysetFromSigner(s0, self.tempdir)
    self.assertTrue(s0.CheckKeyset(ks0))

  def testCheckRequiredPrivateKeysMissing(self):
    ks_empty = keys.Keyset()
    s0 = MockBaseSigner(required_keys_private=['key1'])
    self.assertFalse(s0.CheckKeyset(ks_empty))

  def testCheckRequiredPrivateKeys(self):
    s0 = MockBaseSigner(required_keys_private=['key1'])
    ks0 = KeysetFromSigner(s0, self.tempdir)
    self.assertTrue(s0.CheckKeyset(ks0))

  def testCheckRequiredKeyblocksEmpty(self):
    ks_empty = keys.Keyset()
    s0 = MockBaseSigner(required_keyblocks=['key1'])
    self.assertFalse(s0.CheckKeyset(ks_empty))

  def testCheckRequiredKeyblocks(self):
    s0 = MockBaseSigner(required_keyblocks=['key1'])
    ks0 = KeysetFromSigner(s0, self.tempdir)
    self.assertTrue(s0.CheckKeyset(ks0))


def KeysetFromSigner(s, keydir):
  """Returns a valid keyset containing required keys and keyblocks."""
  ks = keys.Keyset()

  # pylint: disable=protected-access
  for key_name in s._required_keys:
    key = keys.KeyPair(key_name, keydir=keydir)
    ks.AddKey(key)
    keys_unittest.CreateDummyKeys(key)

  for key_name in s._required_keys_public:
    key = keys.KeyPair(key_name, keydir=keydir)
    ks.AddKey(key)
    keys_unittest.CreateDummyPublic(key)

    if key in s._required_keyblocks:
      keys_unittest.CreateDummyKeyblock(key)

  for key_name in s._required_keys_private:
    key = keys.KeyPair(key_name, keydir=keydir)
    ks.AddKey(key)
    keys_unittest.CreateDummyPrivateKey(key)

  for keyblock_name in s._required_keyblocks:
    if keyblock_name not in ks.keys:
      ks.AddKey(keys.KeyPair(keyblock_name, keydir=keydir))

    key = ks.keys[keyblock_name]
    keys_unittest.CreateDummyKeyblock(key)

  return ks


class MockFutilitySigner(signer.FutilitySigner):
  """Basic implementation of a FutilitySigner."""
  _required_keys = ('foo',)

  def GetFutilityArgs(self, keyset, input_name, output_name):
    """Returns a list of [input_name, output_name]."""
    return [input_name, output_name]


class TestFutilitySigner(cros_test_lib.RunCommandTestCase,
                         cros_test_lib.TempDirTestCase):
  """Test Futility Signer."""

  def testSign(self):
    keyset = keys.Keyset()
    fs = signer.FutilitySigner()
    self.assertRaises(NotImplementedError, fs.Sign, keyset, 'dummy', 'dummy')

  def testSignWithMock(self):
    foo_key = keys.KeyPair('foo', self.tempdir)
    keys_unittest.CreateDummyKeys(foo_key)

    keyset = keys.Keyset()
    keyset.AddKey(foo_key)

    fsm = MockFutilitySigner()
    self.assertTrue(fsm.Sign(keyset, 'foo', 'bar'))
    self.assertCommandContains(['foo', 'bar'])

  def testSignWithMockMissingKey(self):
    keyset = keys.Keyset()
    fsm = MockFutilitySigner()
    self.assertFalse(fsm.Sign(keyset, 'foo', 'bar'))

  def testGetCmdArgs(self):
    keyset = keys.Keyset()
    fs = signer.FutilitySigner()
    self.assertRaises(NotImplementedError,
                      fs.GetFutilityArgs, keyset, 'foo', 'bar')


class TestFutilityFunction(cros_test_lib.RunCommandTestCase):
  """Test Futility command."""

  def testCommand(self):
    self.assertTrue(signer.RunFutility([]),
                    msg='Futility should pass w/ mock')
    self.assertCommandContains(['futility'])

  def testCommandWithArgs(self):
    args = ['--privkey', 'foo.priv2']
    signer.RunFutility(args)
    self.assertCommandContains(args)

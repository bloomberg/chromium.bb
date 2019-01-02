# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""""Unittest ChromeOS image signer logic"""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.signing.lib import keys
from chromite.signing.lib import signer
from chromite.signing.lib import keys_unittest

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
    s0 = MockBaseSigner(required_keyblocks=['keyblock1'])
    self.assertFalse(s0.CheckKeyset(ks_empty))

  def testCheckRequiredKeyblocks(self):
    s0 = MockBaseSigner(required_keyblocks=['keyblock1'])
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

  for key_name in s._required_keys_private:
    key = keys.KeyPair(key_name, keydir=keydir)
    ks.AddKey(key)
    keys_unittest.CreateDummyPrivateKey(key)

  for keyblock_name in s._required_keyblocks:
    keyblock = keys.Keyblock(keyblock_name, keydir=keydir)
    ks.AddKeyblock(keyblock)
    keys_unittest.CreateDummyKeyblock(keyblock)

  return ks

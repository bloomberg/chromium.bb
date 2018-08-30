# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""""ChromeOS key unittests"""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.signing.lib import keys


class KeysetMock(keys.Keyset):
  """Mock Keyset that mimics common loem key directory."""
  KEYS = ('ec_data_key',
          'ec_root_key',
          'firmware_data_key',
          'installer_kernel_data_key',
          'kernel_data_key',
          'recovery_kernel_data_key',
          'root_key')

  KEYS_WITH_SUBKEYS = ('firmware_data_key', 'root_key')

  SUBKEYS = ('loem1', 'loem2', 'loem3', 'loem4')

  ALIAS = ('ACME', 'SHINRA')

  def __init__(self, keydir):
    super(KeysetMock, self).__init__()

    self.keydir = keydir

    for key_name in KeysetMock.KEYS:
      self.AddKey(keys.KeyPair(key_name, keydir))

    for key_name in KeysetMock.KEYS_WITH_SUBKEYS:
      for subkey_name in KeysetMock.SUBKEYS:
        self.AddSubkey(key_name, subkey_name)

    for alias, subkey_name in zip(KeysetMock.ALIAS, KeysetMock.SUBKEYS):
      self.subkey_aliases[alias] = subkey_name

  def WriteIniFile(self):
    """Writes alias to file"""
    with open(os.path.join(self.keydir, 'loem.ini'), 'w') as conf:
      conf.write('[loem]\n'
                 '1 = ACME\n'
                 '2 = SHINRA')

  def CreateDummyKeys(self):
    """Creates dummy keys from stored keys."""
    for key in self.keys.values():
      CreateDummyKeys(key)


class TestKeyPair(cros_test_lib.TempDirTestCase):
  """Test KeyPair class."""

  def testInitSimple(self):
    """Test init with minimal arguments."""
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.name, 'key1')
    self.assertEqual(k1.version, 1)
    self.assertEqual(k1.keydir, self.tempdir)

  def testPrivateKey(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.private,
                     os.path.join(self.tempdir, 'key1' + '.vbprivk'))

  def testPublicKey(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.public,
                     os.path.join(self.tempdir, 'key1' + '.vbpubk'))

  def testKeyblock(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.keyblock,
                     os.path.join(self.tempdir, 'key1' + '.keyblock'))

  def testKeyblockWithSuffix(self):
    k1 = keys.KeyPair('key1_data_key', self.tempdir)
    self.assertEqual(k1.keyblock,
                     os.path.join(self.tempdir, 'key1' + '.keyblock'))

  def testInitWithVersion(self):
    """Test init with version kwarg."""
    k_ver = keys.KeyPair('k_ver', self.tempdir, version=2)
    self.assertEqual(k_ver.version, 2)

  def testInitWithPubExt(self):
    """Test init with pub_ext kwarg."""
    k_ext = keys.KeyPair('k_ext', self.tempdir, pub_ext='.vbpubk2')
    self.assertEqual(k_ext.public,
                     os.path.join(self.tempdir, 'k_ext.vbpubk2'))

  def testInitWithPrivExt(self):
    """Test init with priv_ext kwarg."""
    k_ext = keys.KeyPair('k_ext', self.tempdir, priv_ext='.vbprivk2')
    self.assertEqual(k_ext.private,
                     os.path.join(self.tempdir, 'k_ext.vbprivk2'))

  def testCmpSame(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    k2 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1, k1)
    self.assertEqual(k1, k2)

  def testCmpDiff(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    k2 = keys.KeyPair('key2', self.tempdir)
    self.assertNotEqual(k1, k2)

  def testAddSubkey(self):
    k1 = keys.KeyPair('key1', self.tempdir)

    k1.AddSubkey('loem1')
    k_loem1 = k1.subkeys['loem1']
    self.assertIsInstance(k_loem1, keys.KeyPair)
    self.assertEqual(k_loem1.name, 'key1.loem1')
    self.assertEqual(k1.keydir, k_loem1.keydir)
    self.assertEqual(k1.version, k_loem1.version)

  def testExistsEmpty(self):
    self.assertFalse(keys.KeyPair('key1', self.tempdir).Exists())

  def testExistEmptyRequirePublic(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertFalse(k1.Exists(require_public=True))

  def testExistEmptyRequirePrivate(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertFalse(k1.Exists(require_private=True))

  def testExistEmptyRequirePublicRequirePrivate(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertFalse(k1.Exists(require_private=True, require_public=True))

  def testExistWithPublicKey(self):
    k1 = keys.KeyPair('key1', self.tempdir)

    CreateDummyPublic(k1)
    self.assertTrue(k1.Exists())
    self.assertTrue(k1.Exists(require_public=True))
    self.assertFalse(k1.Exists(require_private=True))

  def testExistsWithPrivateKey(self):
    k1 = keys.KeyPair('key1', self.tempdir)

    CreateDummyPrivateKey(k1)
    self.assertTrue(k1.Exists())
    self.assertTrue(k1.Exists(require_private=True))
    self.assertFalse(k1.Exists(require_public=True))

  def testKeyblockExistsMissing(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertFalse(k1.KeyblockExists())

  def testKeyblockExists(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    CreateDummyKeyblock(k1)
    self.assertTrue(k1.KeyblockExists())


class TestKeyset(cros_test_lib.TempDirTestCase):
  """Test Keyset class."""

  def _get_keyset(self):
    """Returns keyset with a few keys."""
    kc = keys.Keyset()

    for key_name in ['key1', 'key2', 'key3', 'key4']:
      kc.AddKey(keys.KeyPair(key_name, self.tempdir))

    kc.subkey_aliases = {'board1':'loem1',
                         'board2':'loem2'}

    for subkey in kc.subkey_aliases:
      kc.AddSubkey('key4', subkey)

    return kc

  def testInit(self):
    ks = keys.Keyset()
    self.assertIsInstance(ks.keys, dict)
    self.assertIsInstance(ks.subkey_aliases, dict)

  def testEqSame(self):
    kc1 = self._get_keyset()
    kc2 = self._get_keyset()
    self.assertTrue(kc1 == kc2)

  def testEqDiffrent(self):
    kc1 = self._get_keyset()
    kc2 = self._get_keyset()

    kc2 = self._get_keyset()
    kc2.AddKey(keys.KeyPair('Foo', self.tempdir))

    self.assertFalse(kc1 == kc2)

  def testAddKey(self):
    ks0 = keys.Keyset()
    key0 = keys.KeyPair('key0', self.tempdir)
    ks0.AddKey(key0)
    self.assertEqual(ks0.keys['key0'], key0)

  def testAddKeyWithKeyName(self):
    ks0 = keys.Keyset()
    key0 = keys.KeyPair('key0', self.tempdir)
    ks0.AddKey(key0, key_name='key0_new')
    self.assertEqual(ks0.keys['key0_new'], key0)

  def testAddSubkey(self):
    ks0 = self._get_keyset()
    ks0.AddSubkey('key1', 'sub1')

    self.assertIsInstance(ks0.keys['key1'].subkeys['sub1'], keys.KeyPair)

  def testAddSubkeyWithAlias(self):
    ks0 = self._get_keyset()
    ks0.subkey_aliases['sub1'] = 'loem1'
    ks0.AddSubkey('key1', 'sub1')

    self.assertIsInstance(ks0.keys['key1'].subkeys['loem1'], keys.KeyPair)

  def testGetSubKeysetMissmatch(self):
    ks0 = self._get_keyset()
    ks1 = ks0.GetSubKeyset('foo')

    self.assertIsInstance(ks1, keys.Keyset)
    self.assertDictEqual(ks0.keys, ks1.keys)

  def testGetSubKeyset(self):
    ks0 = self._get_keyset()
    ks1 = ks0.GetSubKeyset('loem1')
    self.assertEqual(ks0.keys['key4'].subkeys['loem1'], ks1.keys['key4'])

  def testGetSubKeysetWithAliases(self):
    ks0 = self._get_keyset()
    ks1 = ks0.GetSubKeyset('board2')
    self.assertEqual(ks0.keys['key4'].subkeys['loem2'], ks1.keys['key4'])

  def testPrune(self):
    ks0 = self._get_keyset()
    key_keep = ['key1', 'key3']

    for key_name in key_keep:
      CreateDummyKeys(ks0.keys[key_name])

    ks0.Prune()
    for key_name, key in ks0.keys.items():
      self.assertTrue(key.Exists(), msg='All keys should exist')
      self.assertIn(key_name, key_keep,
                    msg='Only keys in key_keep should exists')

  def testKeyExistsMissing(self):
    ks0 = self._get_keyset()

    self.assertFalse(ks0.KeyExists('foo'), msg="'foo' should not exist")
    self.assertFalse(ks0.KeyExists('key1'), msg="'key1' should not exist")

  def testKeyExistsPublicAndPrivate(self):
    ks0 = self._get_keyset()
    CreateDummyKeys(ks0.keys['key1'])
    self.assertTrue(ks0.KeyExists('key1'), msg="key1 should exist")
    self.assertTrue(ks0.KeyExists('key1', require_public=True),
                    msg="key1 public key should exist")
    self.assertTrue(ks0.KeyExists('key1', require_private=True),
                    msg="key1 private key should exist")
    self.assertTrue(ks0.KeyExists('key1',
                                  require_public=True,
                                  require_private=True),
                    msg="key1 keys should exist")

  def testKeyExistsPrivate(self):
    ks0 = self._get_keyset()
    CreateDummyPrivateKey(ks0.keys['key2'])
    self.assertTrue(ks0.KeyExists('key2'),
                    msg="should pass with only private key")
    self.assertTrue(ks0.KeyExists('key2', require_private=True),
                    msg="Should exist with only private key")
    self.assertFalse(ks0.KeyExists('key2', require_public=True),
                     msg="Shouldn't pass with only private key")

  def testKeyExistsPublic(self):
    ks0 = self._get_keyset()
    CreateDummyPublic(ks0.keys['key3'])
    self.assertTrue(ks0.KeyExists('key3'),
                    msg="should pass with only public key")
    self.assertTrue(ks0.KeyExists('key3', require_public=True),
                    msg="Should exist with only public key")
    self.assertFalse(ks0.KeyExists('key3', require_private=True),
                     msg="Shouldn't pass with only public key")

  def testKeyblockExistsMissing(self):
    ks0 = self._get_keyset()
    self.assertFalse(ks0.KeyExists('foo'), msg="'foo' should not exist")
    self.assertFalse(ks0.KeyExists('key1'), msg="'key1' not created yet")

  def testKeyblockExists(self):
    ks0 = self._get_keyset()
    CreateDummyKeyblock(ks0.keys['key1'])
    self.assertTrue(ks0.KeyblockExists('key1'), msg="'key1' should exist")


class testKeysetFromDir(cros_test_lib.TempDirTestCase):
  """Test keyeset_from_dir function."""

  def testEmpty(self):
    ks = keys.KeysetFromDir(self.tempdir)
    self.assertIsInstance(ks, keys.Keyset)

  def testWithKeysetMock(self):
    ks0 = KeysetMock(self.tempdir)
    ks0.CreateDummyKeys()
    ks0.WriteIniFile()

    ks1 = keys.KeysetFromDir(self.tempdir)

    self.assertDictEqual(ks0.keys, ks1.keys, msg='Incorrect keys')

    for key in KeysetMock.KEYS_WITH_SUBKEYS:
      self.assertDictEqual(ks0.keys[key].subkeys, ks1.keys[key].subkeys,
                           msg='Incorrect subkeys')

    self.assertDictEqual(ks0.subkey_aliases, ks1.subkey_aliases,
                         msg='Incorrect key aliases')

    self.assertEqual(ks0, ks1)


def CreateDummyPublic(key):
  """Create empty public key file for given key (or subkeys if exist)."""
  if key.subkeys:
    for subkey in key.subkeys.values():
      CreateDummyPublic(subkey)
  else:
    osutils.Touch(key.public)


def CreateDummyPrivateKey(key):
  """Create empty private key for given key (or subkeys if exist)."""
  if key.subkeys:
    for subkey in key.subkeys.values():
      CreateDummyPrivateKey(subkey)
  else:
    osutils.Touch(key.private)


def CreateDummyKeyblock(key):
  """Create empty keyblock file for given key (or subkeys if exist)."""
  if key.subkeys:
    for subkey in key.subkeys.values():
      CreateDummyKeyblock(subkey)
  else:
    osutils.Touch(key.keyblock)


def CreateDummyKeys(key):
  """Create empty key files for given key (or subkeys if exist)."""
  CreateDummyPublic(key)
  CreateDummyPrivateKey(key)
  CreateDummyKeyblock(key)

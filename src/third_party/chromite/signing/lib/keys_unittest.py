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


class TestKeyPair(cros_test_lib.TempDirTestCase):
  """Test KeyPair class."""

  def testInitSimple(self):
    """Test init with minimal arguments."""
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.name, 'key1')
    self.assertEqual(k1.version, 1)
    self.assertEqual(k1.keydir, self.tempdir)
    self.assertEqual(k1.basename, 'key1')
    self.assertEqual(k1.private,
                     os.path.join(self.tempdir, 'key1' + '.vbprivk'))
    self.assertEqual(k1.public,
                     os.path.join(self.tempdir, 'key1' + '.vbpubk'))

  def testInitWithVersion(self):
    """Test init with version kwarg."""
    k_ver = keys.KeyPair('k_ver', self.tempdir, version=2)
    self.assertEqual(k_ver.version, 2)

  def testInitWithBasename(self):
    """Test init with basename kwarg."""
    k_basename = 'k_base_new'
    k_base = keys.KeyPair('k_base', self.tempdir, basename=k_basename)
    self.assertEqual(k_base.basename, k_basename)
    self.assertEqual(k_base.private,
                     os.path.join(self.tempdir, k_basename + '.vbprivk'))
    self.assertEqual(k_base.public,
                     os.path.join(self.tempdir, k_basename + '.vbpubk'))

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

  def testCreateKeyblockSimple(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    kb1 = k1.CreateKeyblock()
    self.assertEqual(kb1.keydir, self.tempdir)
    self.assertEqual(kb1.name, k1.name)
    self.assertEqual(kb1.data_key, k1)


  def testCreateKeyblockWithName(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    kb2 = k1.CreateKeyblock(name='kb2')
    self.assertEqual(kb2.name, 'kb2')

  def testCreateKeyblockWithBasename(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    kb1 = k1.CreateKeyblock(basename='bk1_new')
    self.assertEqual(kb1.filename,
                     os.path.join(self.tempdir, 'bk1_new.keyblock'))

  def testCreateKeyblockWithDataKey(self):
    key4 = keys.KeyPair('key4_data_key', self.tempdir)
    kb4 = key4.CreateKeyblock()
    self.assertEqual(kb4.name, 'key4')
    self.assertEqual(kb4.filename, os.path.join(self.tempdir, 'key4.keyblock'))


class TestKeyblock(cros_test_lib.TempDirTestCase):
  """Test Keyblock class"""

  def testInitSimple(self):
    kb0 = keys.Keyblock('kb0', keydir=self.tempdir)
    self.assertEqual(kb0.name, 'kb0')
    self.assertEqual(kb0.keydir, self.tempdir)
    self.assertEqual(kb0.filename, os.path.join(self.tempdir, 'kb0.keyblock'))
    self.assertEqual(kb0.data_key, None)

  def testInitNoKeydir(self):
    self.assertRaises(ValueError, keys.Keyblock, 'kb0')

  def testInitWithBasename(self):
    kb0 = keys.Keyblock('kb0', self.tempdir, basename='kb0_new')
    self.assertEqual(kb0.filename,
                     os.path.join(self.tempdir, 'kb0_new.keyblock'))

  def testInitWithKey(self):
    key0 = keys.KeyPair('key1', self.tempdir)
    kb0 = keys.Keyblock('kb0', data_key=key0)
    self.assertEqual(kb0.data_key, key0)
    self.assertEqual(kb0.keydir, self.tempdir)

  def testExistsEmpty(self):
    kb0 = keys.Keyblock('kb0', keydir=self.tempdir)
    self.assertFalse(kb0.Exists())

  def testExists(self):
    kb0 = keys.Keyblock('kb0', self.tempdir)
    CreateDummyKeyblock(kb0)
    self.assertTrue(kb0.Exists())


class TestKeyset(cros_test_lib.TempDirTestCase):
  """Test Keyset class."""

  def _get_keyset(self):
    """Returns keyset with a few keys."""
    kc = keys.Keyset()

    for key_name in ['key1', 'key2', 'key3', 'key4']:
      kc.AddKey(keys.KeyPair(key_name, self.tempdir))

    for keyblock_name, key_name in [('kb1', 'key1'),
                                    ('kb2', 'key2'),
                                    ('kb4', 'key4')]:
      kc.AddKeyblock(kc.keys[key_name].CreateKeyblock(name=keyblock_name))

    return kc

  def testInit(self):
    ks = keys.Keyset()
    self.assertIsInstance(ks.keys, dict)
    self.assertIsInstance(ks.keyblocks, dict)

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

  def testGetKeyMissing(self):
    ks0 = self._get_keyset()
    self.assertIsNone(ks0.GetKey('foo'))

  def testGetKey(self):
    ks0 = self._get_keyset()
    self.assertEqual(ks0.GetKey('key1'), ks0.keys['key1'])

  def testAddKeyblock(self):
    ks0 = keys.Keyset()
    kb0 = keys.Keyblock('keyblock0', keydir=self.tempdir)
    ks0.AddKeyblock(kb0)
    self.assertEqual(ks0.keyblocks['keyblock0'], kb0)

  def testAddKeyblockWithName(self):
    ks0 = keys.Keyset()
    kb0 = keys.Keyblock('keyblock0', keydir=self.tempdir)
    ks0.AddKeyblock(kb0, kb_name='keyblock0_new')
    self.assertEqual(ks0.keyblocks['keyblock0_new'], kb0)

  def testGetKeyblockMissing(self):
    ks0 = self._get_keyset()
    self.assertIsNone(ks0.GetKeyblock('foo'))

  def testGetKeyblock(self):
    ks0 = self._get_keyset()
    self.assertEqual(ks0.GetKeyblock('kb1'), ks0.keyblocks['kb1'])

  def testPrune(self):
    ks0 = self._get_keyset()
    key_keep = ['key1', 'key3']
    kb_keep = ['kb1', 'kb4']

    for key_name in key_keep:
      CreateDummyKeys(ks0.GetKey(key_name))

    for kb_name in kb_keep:
      CreateDummyKeyblock(ks0.GetKeyblock(kb_name))

    ks0.Prune()
    for key_name, key in ks0.keys.iteritems():
      self.assertTrue(key.Exists(), msg='All keys should exist')
      self.assertIn(key_name, key_keep,
                    msg='Only keys in key_keep should exists')

    for keyblock_name, keyblock in ks0.keyblocks.iteritems():
      self.assertTrue(keyblock.Exists(), msg='All keyblocks should exist')
      self.assertIn(keyblock_name, kb_keep,
                    msg='Only keyblocks in kb_keep should exist')

  def testKeyExistsMissing(self):
    ks0 = self._get_keyset()

    self.assertFalse(ks0.KeyExists('foo'), msg="'foo' should not exist")
    self.assertFalse(ks0.KeyExists('key1'), msg="'foo' should not exist")

  def testKeyExistsPublicAndPrivate(self):
    ks0 = self._get_keyset()
    CreateDummyKeys(ks0.GetKey('key1'))
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
    CreateDummyPrivateKey(ks0.GetKey('key2'))
    self.assertTrue(ks0.KeyExists('key2'),
                    msg="should pass with only private key")
    self.assertTrue(ks0.KeyExists('key2', require_private=True),
                    msg="Should exist with only private key")
    self.assertFalse(ks0.KeyExists('key2', require_public=True),
                     msg="Shouldn't pass with only private key")

  def testKeyExistsPublic(self):
    ks0 = self._get_keyset()
    CreateDummyPublic(ks0.GetKey('key3'))
    self.assertTrue(ks0.KeyExists('key3'),
                    msg="should pass with only public key")
    self.assertTrue(ks0.KeyExists('key3', require_public=True),
                    msg="Should exist with only public key")
    self.assertFalse(ks0.KeyExists('key3', require_private=True),
                     msg="Shouldn't pass with only public key")

  def testKeyblockExistsMissing(self):
    ks0 = self._get_keyset()
    self.assertFalse(ks0.KeyExists('foo'), msg="'foo' should not exist")
    self.assertFalse(ks0.KeyExists('kb1'), msg="'kb1' not created yet")

  def testKeyblockExists(self):
    ks0 = self._get_keyset()
    CreateDummyKeyblock(ks0.GetKeyblock('kb1'))
    self.assertTrue(ks0.KeyblockExists('kb1'), msg="'kb1' should exist")


class testKeysetFromDir(cros_test_lib.TempDirTestCase):
  """Test keyeset_from_dir function."""

  dev_keys = ['ec_data_key',
              'ec_root_key',
              'firmware_data_key',
              'installer_kernel_data_key',
              'kernel_data_key',
              'recovery_kernel_data_key',
              'root_key']

  dev_keyblocks = ['ec',
                   'firmware',
                   'installer_kernel',
                   'kernel',
                   'recovery_kernel']

  dev_loem_keys = ['firmware_data_key.loem1',
                   'firmware_data_key.loem2',
                   'firmware_data_key.loem3',
                   'firmware_data_key.loem4',
                   'root_key.loem1',
                   'root_key.loem2',
                   'root_key.loem3',
                   'root_key.loem4']

  dev_loem_keyblocks = ['firmware.loem1',
                        'firmware.loem2',
                        'firmware.loem3',
                        'firmware.loem4',
                        'root_key.loem1',
                        'root_key.loem2',
                        'root_key.loem3',
                        'root_key.loem4']

  def _create_keyfiles(self, dev_keys, dev_keyblocks=None):
    """Helper to create dummy keyfiles."""
    for key_name in dev_keys:
      key = keys.KeyPair(key_name, self.tempdir)
      CreateDummyPrivateKey(key)

      if dev_keyblocks:
        kb = key.CreateKeyblock()
        if kb.name in dev_keyblocks:
          CreateDummyKeyblock(kb)

  def testEmpty(self):
    ks = keys.KeysetFromDir(self.tempdir)
    self.assertIsInstance(ks, keys.Keyset)

  def testWithKeypair(self):
    self._create_keyfiles(self.dev_keys)

    ks = keys.KeysetFromDir(self.tempdir)
    for key_name in self.dev_keys:
      self.assertTrue(ks.KeyExists(key_name))

    self.assertEqual(ks.keyblocks, {}, msg="keyblocks should be empty")

  def testCompleteDir(self):
    self._create_keyfiles(self.dev_keys, self.dev_keyblocks)

    ks = keys.KeysetFromDir(self.tempdir)
    for key_name in self.dev_keys:
      self.assertTrue(ks.KeyExists(key_name))

    for kb_name in self.dev_keyblocks:
      self.assertTrue(ks.KeyblockExists(kb_name))

  def testLoemDir(self):
    self._create_keyfiles(self.dev_loem_keys, self.dev_loem_keyblocks)

    ks = keys.KeysetFromDir(self.tempdir)
    for key_name in self.dev_loem_keys:
      self.assertTrue(ks.KeyExists(key_name))

    for kb_name in self.dev_loem_keyblocks:
      self.assertTrue(ks.KeyblockExists(kb_name))

def CreateDummyPublic(key):
  """Create empty public key file for testing."""
  osutils.Touch(key.public)


def CreateDummyPrivateKey(key):
  """Create empty private key file for testing."""
  osutils.Touch(key.private)


def CreateDummyKeys(key):
  """Create empty key files for testing."""
  CreateDummyPublic(key)
  CreateDummyPrivateKey(key)


def CreateDummyKeyblock(keyblock):
  """Create empty keyblock file for testing."""
  osutils.Touch(keyblock.filename)

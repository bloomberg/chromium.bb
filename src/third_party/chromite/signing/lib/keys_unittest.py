# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""""ChromeOS key unittests"""

from __future__ import print_function

import os
import textwrap

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.signing.lib import keys


MOCK_SHA1SUM = 'e2c1c92d7d7aa7dfed5e8375edd30b7ae52b7450'

# pylint: disable=protected-access

def MockVbutilKey(rc, sha1sum=MOCK_SHA1SUM):
  """Adds vbutil_key mocks to |rc|"""

  cmd_output = textwrap.dedent('''
    Public Key file:   firmware_data_key.vbpubk
    Algorithm:         7 RSA4096 SHA256
    Key Version:       1
    Key sha1sum:       ''' + sha1sum)

  rc.AddCmdResult(partial_mock.ListRegex('vbutil_key --unpack .*'),
                  output=cmd_output)


class KeysetMock(keys.Keyset):
  """Mock Keyset that mimics common loem key directory."""
  KEYS = ('ec_data_key',
          'ec_root_key',
          'key_ec_efs',
          'firmware_data_key',
          'installer_kernel_data_key',
          'kernel_data_key',
          'kernel_subkey',
          'recovery_kernel_data_key',
          'recovery_key',
          'root_key')

  KEYS_WITH_ROOT_OF_TRUST_ALIASES = keys.Keyset._root_of_trust_key_names

  ROOT_OF_TRUST_ALIASES = ('loem1', 'loem2', 'loem3', 'loem4')

  ROOT_OF_TRUST_NAMES = ('ACME', 'SHINRA', 'WILE', 'COYOTE')

  def __init__(self, key_dir, has_loem_ini=True):
    """Create a Keyset with root_of_trust-specific keys, and populate it."""

    # This will create the Keyset without root_of_trust-specific keys, since
    # that is determined by having loem.ini, and the directory does not exist
    # yet, let alone loem.ini.
    # We do not actually create files for the KeyPairs, since the tests care.
    super(KeysetMock, self).__init__(key_dir)

    osutils.SafeMakedirs(key_dir)
    self.has_loem_ini = has_loem_ini

    # By default, we have root_of_trust-specific keys.  If not, don't create
    # them.
    if not has_loem_ini:
      self.KEYS_WITH_ROOT_OF_TRUST_ALIASES = ()
      self.ROOT_OF_TRUST_ALIASES = ()
      self.ROOT_OF_TRUST_NAMES = ()

    # Create KeyPairs as appropriate for the Keyset we are mocking.
    for key_name in KeysetMock.KEYS:
      if key_name not in self.KEYS_WITH_ROOT_OF_TRUST_ALIASES:
        self.AddKey(keys.KeyPair(key_name, key_dir))

    for name in self.KEYS_WITH_ROOT_OF_TRUST_ALIASES:
      for root_of_trust in self.ROOT_OF_TRUST_ALIASES:
        key = keys.KeyPair("%s.%s" % (name, root_of_trust), key_dir)
        self.AddRootOfTrustKey(name, root_of_trust, key)

    for alias, root_of_trust in zip(
        self.ROOT_OF_TRUST_NAMES, self.ROOT_OF_TRUST_ALIASES):
      self.root_of_trust_map[alias] = root_of_trust
      self.root_of_trust_map[root_of_trust] = root_of_trust
    self.WriteIniFile()

  # TODO(lamontjones): This may eventually make sense to move into
  # keys.Keyset(), if it ever becomes the thing that creates a keyset directory.
  # Today, we only read the keyset from the directory, we do not update it.
  def WriteIniFile(self):
    """Writes alias to file"""
    if not self.has_loem_ini:
      return
    lines = ['[loem]']
    lines += ['%d = %s' % (i, name)
              for i, name in enumerate(self.ROOT_OF_TRUST_NAMES, 1)]
    contents = '\n'.join(lines) + '\n'
    osutils.WriteFile(os.path.join(self.key_dir, 'loem.ini'), contents)

  def CreateDummyKeys(self):
    """Creates dummy keys from stored keys."""
    for key in self.keys.values():
      CreateDummyKeys(key)
    for loem in self._root_of_trust_keys.values():
      for key in loem.values():
        CreateDummyKeys(key)


class TestKeyPair(cros_test_lib.RunCommandTempDirTestCase):
  """Test KeyPair class."""

  def testInitSimple(self):
    """Test init with minimal arguments."""
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1.name, 'key1')
    self.assertEqual(k1.version, 1)
    self.assertEqual(k1.keydir, self.tempdir)

  def testRejectsEmptyName(self):
    with self.assertRaises(ValueError):
      keys.KeyPair('', self.tempdir)

  def testRejectsNameWithSlash(self):
    with self.assertRaises(ValueError):
      keys.KeyPair('/foo', self.tempdir)
    with self.assertRaises(ValueError):
      keys.KeyPair('foo/bar', self.tempdir)

  def testRejectsLeadingDot(self):
    with self.assertRaises(ValueError):
      keys.KeyPair('.foo', self.tempdir)

  def testCoercesVersionToInt(self):
    k1 = keys.KeyPair('key1', self.tempdir, version='1')
    self.assertEqual(k1.name, 'key1')
    self.assertEqual(k1.version, 1)
    self.assertEqual(k1.keydir, self.tempdir)

  def testAssertsValueErrorOnNonNumericVersion(self):
    with self.assertRaises(ValueError):
      keys.KeyPair('key1', self.tempdir, version='blah')

  def testAssertsValueErrorOnEmptyStringVersion(self):
    with self.assertRaises(ValueError):
      keys.KeyPair('key1', self.tempdir, version='')

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
    k_ext = keys.KeyPair('k_ext', self.tempdir, priv_ext='.vbprik2')
    self.assertEqual(k_ext.private,
                     os.path.join(self.tempdir, 'k_ext.vbprik2'))
    self.assertEqual(k_ext.public,
                     os.path.join(self.tempdir, 'k_ext.vbpubk2'))

  def testRejectsInvalidPubExt(self):
    """Test init with bad pub_ext argument."""
    with self.assertRaises(ValueError):
      keys.KeyPair('foo', self.tempdir, pub_ext='.bar')

  def testRejectsInvalidPrivExt(self):
    """Test init with bad priv_ext argument."""
    with self.assertRaises(ValueError):
      keys.KeyPair('foo', self.tempdir, priv_ext='.bar')

  def testSetsPubExtCorrectly(self):
    """Test init sets pub_ext correctly based on priv_ext argument."""
    k1 = keys.KeyPair('k1', self.tempdir, priv_ext='.vbprivk')
    k2 = keys.KeyPair('k2', self.tempdir, priv_ext='.vbprik2')
    self.assertEqual(k1._pub_ext, '.vbpubk')
    self.assertEqual(k2._pub_ext, '.vbpubk2')
    self.assertEqual(k1.public, '%s/k1.vbpubk' % self.tempdir)
    self.assertEqual(k2.public, '%s/k2.vbpubk2' % self.tempdir)

  def testCmpSame(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    k2 = keys.KeyPair('key1', self.tempdir)
    self.assertEqual(k1, k1)
    self.assertEqual(k1, k2)

  def testCmpDiff(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    k2 = keys.KeyPair('key2', self.tempdir)
    self.assertNotEqual(k1, k2)

  def testParsePrivateKeyFilenameReturnsValues(self):
    """Make sure that we return the correct name/ext."""
    v1 = keys.KeyPair.ParsePrivateKeyFilename('foo.vbprivk')
    self.assertEqual('foo', v1.group('name'))
    self.assertEqual('.vbprivk', v1.group('ext'))
    v2 = keys.KeyPair.ParsePrivateKeyFilename('bar.vbprik2')
    self.assertEqual('bar', v2.group('name'))
    self.assertEqual('.vbprik2', v2.group('ext'))

  def testParsePrivateKeyFilenameReturnsNone(self):
    """Non-private key filenames return None"""
    self.assertEqual(None, keys.KeyPair.ParsePrivateKeyFilename('foo.vbpubk'))

  def testParsePrivateKeyFilenameStripsDir(self):
    """Leading directories in the path are ignored."""
    name = keys.KeyPair.ParsePrivateKeyFilename(
        '/path/to/foo.vbprivk').group('name')
    self.assertEqual('foo', name)

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

  def testExistsWithBothKeys(self):
    """Exists() works correctly when private/public are both required."""
    k1 = keys.KeyPair('key1', self.tempdir)

    CreateDummyPrivateKey(k1)
    CreateDummyPublic(k1)
    self.assertTrue(k1.Exists())
    self.assertTrue(k1.Exists(require_private=True))
    self.assertTrue(k1.Exists(require_public=True))
    self.assertTrue(k1.Exists(
        require_private=True, require_public=True))

  def testKeyblockExistsMissing(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    self.assertFalse(k1.KeyblockExists())

  def testKeyblockExists(self):
    k1 = keys.KeyPair('key1', self.tempdir)
    CreateDummyKeyblock(k1)
    self.assertTrue(k1.KeyblockExists())

  def testGetSha1sumEmpty(self):
    """"Test GetSha1sum with bad cmd output."""
    k1 = keys.KeyPair('key1', self.tempdir)

    with self.assertRaises(keys.SignerKeyError):
      k1.GetSHA1sum()

  def testGetSha1sumMockCmd(self):
    """Test GetSha1sum with mock cmd output."""
    MockVbutilKey(self.rc)
    k1 = keys.KeyPair('firmware_data_key', self.tempdir)

    k1sum = k1.GetSHA1sum()
    self.assertEqual(k1sum, MOCK_SHA1SUM)
    self.assertCommandCalled(['vbutil_key', '--unpack', k1.public],
                             error_code_ok=True)


class TestKeyVersions(cros_test_lib.TempDirTestCase):
  """Test KeyVersions class."""

  # used for several tests
  expected = {
      'firmware_key_version': 2,
      'firmware_version': 3,
      'kernel_key_version': 4,
      'kernel_version': 5,
      'random': 6,
  }

  def _CreateVersionsFile(self, values):
    kv_path = os.path.join(self.tempdir, 'key.versions')
    lines = ['%s=%d' % (k, v) for k, v in values.items()]
    contents = '\n'.join(lines) + '\n'
    osutils.WriteFile(kv_path, contents)
    return kv_path

  def testInitReturnsDefaultButDoesNotCreateFile(self):
    kv_path = os.path.join(self.tempdir, 'key.versions')
    kv = keys.KeyVersions(kv_path)
    self.assertFalse(os.path.exists(kv_path))
    expected = {
        'firmware_key_version': 1,
        'firmware_version': 1,
        'kernel_key_version': 1,
        'kernel_version': 1,
    }
    self.assertEqual(False, kv.saved)
    self.assertDictEqual(expected, kv._versions)

  def testInitReadsFile(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    self.assertDictEqual(self.expected, kv._versions)
    self.assertEqual(True, kv.saved)

  def testInitErrorOnBadFileContents(self):
    kv_path = self._CreateVersionsFile({})
    osutils.WriteFile(kv_path, 'firmware_version=bogus\n')
    with self.assertRaises(ValueError):
      keys.KeyVersions(kv_path)

  def testKeyNameTransformsName(self):
    kv_path = self._CreateVersionsFile({})
    kv = keys.KeyVersions(kv_path)
    self.assertEqual('firmware_version', kv._KeyName('firmware_data_key'))
    self.assertEqual('firmware_version', kv._KeyName('firmware_data_key.loem1'))
    self.assertEqual('firmware_version', kv._KeyName('firmware_version'))

  def testKeyNameIsIdempotent(self):
    kv_path = self._CreateVersionsFile({})
    kv = keys.KeyVersions(kv_path)
    self.assertEqual('B_version', kv._KeyName(kv._KeyName('B_data_key')))
    self.assertEqual('B_version', kv._KeyName(kv._KeyName('B_data_key.loem1')))
    self.assertEqual('B_version', kv._KeyName(kv._KeyName('B_version')))

  def testGetReturnsValue(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    self.assertEqual(
        self.expected['firmware_version'], kv.Get('firmware_data_key'))

  def testGetReturnsOneForUnknown(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    self.assertEqual(1, kv.Get('invalid'))

  def testSetSetsValue(self):
    kv_path = self._CreateVersionsFile({})
    kv = keys.KeyVersions(kv_path)
    kv.Set('firmware_data_key', 10)
    self.assertEqual(10, kv._versions['firmware_version'])

  def testSetMarksDirty(self):
    kv_path = self._CreateVersionsFile({})
    kv = keys.KeyVersions(kv_path)
    self.assertEqual(True, kv.saved)
    kv.Set('firmware_data_key', 10)
    self.assertEqual(10, kv._versions['firmware_version'])
    self.assertEqual(False, kv.saved)

  def testSetDoesNotSave(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    kv.Set('firmware_data_key', 10)
    kv2 = keys.KeyVersions(kv_path)
    self.assertEqual(
        self.expected['firmware_version'], kv2._versions['firmware_version'])

  def testIncrementIncrementsAndMarksDirty(self):
    kv_path = self._CreateVersionsFile({'firmware_version': 30})
    kv = keys.KeyVersions(kv_path)
    kv.Increment('firmware_data_key')
    self.assertEqual(31, kv._versions['firmware_version'])
    self.assertEqual(False, kv.saved)

  def testIncrementRaisesOnOverflow(self):
    kv_path = self._CreateVersionsFile({'firmware_version': 0xffff})
    kv = keys.KeyVersions(kv_path)
    with self.assertRaises(keys.VersionOverflowError):
      kv.Increment('firmware_data_key')
    self.assertEqual(0xffff, kv._versions['firmware_version'])

  def testIncrementDoesNotSave(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    kv.Increment('firmware_data_key')
    kv2 = keys.KeyVersions(kv_path)
    self.assertEqual(
        self.expected['firmware_version'], kv2._versions['firmware_version'])

  def testSaveSaves(self):
    kv_path = self._CreateVersionsFile(self.expected)
    kv = keys.KeyVersions(kv_path)
    kv.Increment('firmware_data_key')
    kv.Set('new', 37)
    kv.Save()
    self.assertEqual(True, kv.saved)
    kv2 = keys.KeyVersions(kv_path)
    self.assertDictEqual(kv._versions, kv2._versions)


class TestKeyset(cros_test_lib.TempDirTestCase):
  """Test Keyset class."""

  def _get_keyset(self, has_loem_ini=True):
    """Returns keyset with a few keys."""
    kc = KeysetMock(self.tempdir, has_loem_ini=has_loem_ini)

    # Add a few more keys for this build.
    for key_name in ['key1', 'key2', 'key3', 'key4']:
      kc.AddKey(keys.KeyPair(key_name, self.tempdir))

    return kc

  def testInit(self):
    ks = keys.Keyset()
    self.assertIsInstance(ks.keys, dict)
    self.assertIsInstance(ks._root_of_trust_keys, dict)
    self.assertIsInstance(ks.root_of_trust_map, dict)

  def testInitWithEmptyDir(self):
    """Call Keyset() with an uncreated directory."""
    ks = keys.Keyset(self.tempdir)
    self.assertIsInstance(ks, keys.Keyset)
    self.assertIsInstance(ks._root_of_trust_keys, dict)
    self.assertIsInstance(ks.root_of_trust_map, dict)

  def testInitWithPopulatedDirectory(self):
    """Keyset() loads a populated keyset directory correctly."""
    ks0 = KeysetMock(self.tempdir)
    ks0.CreateDummyKeys()

    ks1 = keys.Keyset(self.tempdir)
    self.assertDictEqual(ks0.keys, ks1.keys, msg='Incorrect keys')
    self.assertDictEqual(ks0._root_of_trust_keys, ks1._root_of_trust_keys,
                         msg='Incorrect root_of_trust_keys')
    self.assertDictEqual(ks0.root_of_trust_map, ks1.root_of_trust_map,
                         msg='Incorrect key aliases')
    self.assertEqual(ks0, ks1)

  def testEqSame(self):
    kc1 = self._get_keyset()
    kc2 = self._get_keyset()
    self.assertEqual(kc1, kc2)

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

  def testAddRootOfTrustKey(self):
    k9 = keys.KeyPair('root_key.loem9', self.tempdir)
    ks0 = self._get_keyset()
    ks0.AddRootOfTrustKey('root_key', 'loem9', k9)

    self.assertEqual(ks0._root_of_trust_keys['loem9']['root_key'], k9)

  def testGetRootOfTrustKeysWithLoemIni(self):
    ks0 = self._get_keyset()
    expected_keys = ks0.GetRootOfTrustKeys('root_key')
    expected = {k: ks0._root_of_trust_keys[k]['root_key']
                for k in ks0._root_of_trust_keys.keys()}
    self.assertDictEqual(expected, expected_keys)

  def testGetRootOfTrustKeysWithoutLoemIni(self):
    ks0 = self._get_keyset(has_loem_ini=False)
    expected_keys = ks0.GetRootOfTrustKeys('root_key')
    self.assertDictEqual({'root_key': ks0.keys['root_key']}, expected_keys)

  def testGetBuildKeysetMissmatch(self):
    ks0 = self._get_keyset()

    with self.assertRaises(keys.SignerRootOfTrustKeyMissingError):
      ks0.GetBuildKeyset('foo')

  def testGetBuildKeyset(self):
    ks0 = self._get_keyset()
    ks1 = ks0.GetBuildKeyset('ACME')

    expected_keys = {name: k for name, k in ks0.keys.items()}
    for name, k in ks0._root_of_trust_keys[
        ks0.root_of_trust_map['ACME']].items():
      expected_keys[k.name] = k
      k1 = k.Copy()
      k1.name = name
      expected_keys[name] = k1
    actual_keys = {name: k for name, k in ks1.keys.items()}
    self.assertEqual(expected_keys, actual_keys)
    self.assertEqual(ks1._root_of_trust_keys, {})

  def testGetBuildKeysetWithAliasSucceeds(self):
    ks0 = self._get_keyset()
    ks1 = ks0.GetBuildKeyset('loem3')
    self.assertEqual(ks0._root_of_trust_keys['loem3']['root_key'],
                     ks1.keys['root_key.loem3'])
    self.assertEqual('root_key', ks1.keys['root_key'].name)
    # The rest of the fields should be equal.
    ks1.keys['root_key'].name = 'root_key.loem3'
    self.assertEqual(ks0._root_of_trust_keys['loem3']['root_key'],
                     ks1.keys['root_key'])

  def testGetBuildKeysetWithMissingName(self):
    ks0 = self._get_keyset()
    with self.assertRaises(keys.SignerRootOfTrustKeyMissingError):
      ks0.GetBuildKeyset('loem99')

  def testPrune(self):
    ks0 = self._get_keyset()
    key_keep = set(['key1', 'key3'])

    for key_name in key_keep:
      CreateDummyKeys(ks0.keys[key_name])

    ks0.Prune()
    # Only our keepers should have survived.
    self.assertEqual(key_keep, set([k.name for k in ks0.keys.values()]))
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


def CreateDummyPublic(key):
  """Create empty public key file for given key."""
  osutils.Touch(key.public, makedirs=True)


def CreateDummyPrivateKey(key):
  """Create empty private key for given key."""
  osutils.Touch(key.private, makedirs=True)


def CreateDummyKeyblock(key):
  """Create empty keyblock file for given key."""
  osutils.Touch(key.keyblock, makedirs=True)


def CreateDummyKeys(key):
  """Create empty key files for given key (or root_of_trust_keys if exist)."""
  CreateDummyPublic(key)
  CreateDummyPrivateKey(key)
  CreateDummyKeyblock(key)

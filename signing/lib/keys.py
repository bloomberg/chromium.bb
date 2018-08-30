# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS encryption/decryption key management"""

from __future__ import print_function

import ConfigParser
import os
import re

from chromite.lib import cros_logging as logging


class KeyPair(object):
  """Container for a key's files.

  Attributes:
    name: name of keypair
    keydir: location of key files
    version: version of key
    subkeys: dictionary of sub keys
    public: public key file complete path
    private: private key file complete path
    keyblock: keyblock file complete path
  """

  def __init__(self, name, keydir, version=1, subkeys=(),
               pub_ext='.vbpubk', priv_ext='.vbprivk'):
    """Initialize KeyPair.

    Args:
      name: name of key
      keydir: directory containing key files
      version: version of the key
      subkeys: list of subkeys to create
      pub_ext: file extension used for public key
      priv_ext: file extension used for private key
    """
    self.name = name
    self.keydir = keydir
    self.version = version
    self._pub_ext = pub_ext
    self._priv_ext = priv_ext

    self.subkeys = {}
    for subkey in subkeys:
      self.AddSubkey(subkey)

    self.public = os.path.join(keydir, name + pub_ext)
    self.private = os.path.join(keydir, name + priv_ext)

    #strip '_data_key' from any name, since this is a common convention
    keyblock_name = ''.join(name.split('_data_key'))
    self.keyblock = os.path.join(keydir, keyblock_name + '.keyblock')

  def __eq__(self, other):
    return (isinstance(other, KeyPair)
            and self.name == other.name
            and self.version == other.version
            and self.public == other.public
            and self.private == other.private
            and self.subkeys == other.subkeys)

  def AddSubkey(self, sub_id):
    """Add new Subkey with the given name."""
    if sub_id in self.subkeys:
      return

    self.subkeys[sub_id] = KeyPair(self.name + '.' + sub_id,
                                   self.keydir,
                                   version=self.version,
                                   pub_ext=self._pub_ext,
                                   priv_ext=self._priv_ext)

  def Exists(self, require_public=False, require_private=False):
    """Returns True if ether key or subkeys exists on disk."""

    if self.subkeys:
      for sub_key in self.subkeys.values():
        if not sub_key.Exists(require_public=require_public,
                              require_private=require_private):
          return False
      return True

    has_public = os.path.exists(self.public)
    has_private = os.path.exists(self.private)

    if require_public and not has_public:
      return False
    if require_private and not has_private:
      return False
    return has_public or has_private

  def KeyblockExists(self):
    """Returns if keyblocks or subkeyblocks exist."""
    if self.subkeys:
      for sub_key in self.subkeys.values():
        if not sub_key.KeyblockExists():
          return False
      return True
    else:
      return os.path.exists(self.keyblock)


class Keyset(object):
  """Store signer keys and keyblocks (think keychain).

  Attributes:
    keys: dict of keypairs, indexed on key's name
    subkey_aliases: dict of alias to use for subkeys (i.e. loem -> board)
  """
  def __init__(self):
    self.keys = {}
    self.subkey_aliases = {}

  def __eq__(self, other):
    return (isinstance(other, Keyset)
            and self.subkey_aliases == other.subkey_aliases
            and self.keys == other.keys)

  def Prune(self):
    """Check that all keys exists, else remove them."""
    for k, key in self.keys.items():
      if not key.Exists():
        self.keys.pop(k)

  def AddKey(self, key, key_name=None):
    """Add the given key to keyset, using key.name if key_name is None."""
    key_name = key_name if key_name else key.name

    self.keys[key_name] = key

  def AddSubkey(self, key_name, subkey):
    """Add subkey to key_name, using its alias if available."""
    subkey = (self.subkey_aliases[subkey] if subkey in self.subkey_aliases
              else subkey)
    self.keys[key_name].AddSubkey(subkey)

  def KeyExists(self, key_name, require_public=False, require_private=False):
    """Returns if key is in keyset and exists."""
    return (key_name in self.keys and
            self.keys[key_name].Exists(require_public=require_public,
                                       require_private=require_private))

  def KeyblockExists(self, key_name):
    """Returns if keyblock exists"""
    return (key_name in self.keys and
            self.keys[key_name].KeyblockExists())

  def GetSubKeyset(self, subkey_name):
    """Returns new keyset containing keys based on the subkey_name given.

    Keys are added based on the following rules:
    * Keys does not have a subkey of the given name
    * Subkey exists for the given name, or alias. Key will be indexed under
        it's parent key's name. ex: 'firmware.loem1' -> 'firmware'
    """
    ks = Keyset()

    # Use alias if exists
    subkey_alias = self.subkey_aliases.get(subkey_name, '')

    for key_name, key in self.keys.items():
      if subkey_alias in key.subkeys:
        ks.AddKey(key.subkeys[subkey_alias], key_name=key_name)
      elif subkey_name in key.subkeys:
        ks.AddKey(key.subkeys[subkey_name], key_name=key_name)
      else:
        ks.AddKey(key)

    return ks


def KeysetFromDir(key_dir):
  """Returns a populated keyset generated from given directory.

  Note: every public key and keyblock must have an accompanying private key
  """
  ks = Keyset()

  # Get all loem subkey mappings
  loem_config_filename = os.path.join(key_dir, 'loem.ini')
  if os.path.exists(loem_config_filename):
    logging.info("Reading loem.ini file")
    loem_config = ConfigParser.ConfigParser()
    if loem_config.read(loem_config_filename):
      if loem_config.has_section('loem'):
        for loem_id in loem_config.options('loem'):
          loem_board = loem_config.get('loem', loem_id)
          loem_alias = 'loem' + loem_id
          logging.debug('Adding key alias %s %s', loem_board, loem_alias)
          ks.subkey_aliases[loem_board] = 'loem' + loem_id
    else:
      logging.warning("Error reading loem.ini file")

  # TODO (chingcodes): add versions from file - needed for keygen

  # Match any private key file name
  # Ex: firmware_data_key.loem4.vbprivk, kernel_subkey.vbprivk
  keypair_re = re.compile(r'(?P<name>\w+)(\.(?P<subkey>\w+))?\.vbprivk')

  for f_name in os.listdir(key_dir):
    key_match = keypair_re.match(f_name)
    if key_match:
      key_name = key_match.group('name')

      if key_name not in ks.keys:
        logging.debug('Found new key %s', key_name)
        ks.AddKey(KeyPair(key_name, key_dir, version=1))

      subkey = key_match.group('subkey')
      if subkey:
        logging.debug('Found subkey %s.%s', key_name, subkey)
        ks.AddSubkey(key_name, subkey)

  return ks

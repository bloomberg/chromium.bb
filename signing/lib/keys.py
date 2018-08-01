# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS encryption/decryption key management"""

from __future__ import print_function

import os
import re


class KeyPair(object):
  """Container for a key's files.

  Attributes:
    name: name of keypair
    keydir: location of key files
    basename: common prefix for all key files
    version: version of key
    public: public key file complete path
    private: private key file complete path
  """

  def __init__(self, name, keydir, version=1, basename=None, pub_ext='.vbpubk',
               priv_ext='.vbprivk'):
    """Initialize KeyPair.

    Args:
      name: name of key
      keydir: directory containing key files
      version: version of the key
      basename: basename of key files, default to use name
      pub_ext: file extension used for public key
      priv_ext: file extension used for private key
    """
    self.name = name
    self.keydir = keydir
    self.version = version

    self.basename = name if basename is None else basename

    self.public = os.path.join(self.keydir, self.basename + pub_ext)
    self.private = os.path.join(self.keydir, self.basename + priv_ext)

  def Exists(self, require_public=False, require_private=False):
    """Returns True if either the public or private key exist on disk."""
    has_public = os.path.exists(self.public)
    has_private = os.path.exists(self.private)

    if require_public and not has_public:
      return False
    if require_private and not has_private:
      return False
    return has_public or has_private

  def CreateKeyblock(self, name=None, basename=None):
    """Returns a new Keyblock instance which references this key.

    Args:
      name: keyblock name, defaults to keypair's striping out _data_key
      basename: keyblock basename, defaults to keypair's striping out _data_key
    """

    if name is None:
      # Use keypair's name, striping out '_data_key' if exists
      name = ''.join(self.name.split('_data_key'))

    if basename is None:
      basename = ''.join(self.basename.split('_data_key'))

    return Keyblock(name, basename=basename, data_key=self)


class Keyblock(object):
  """Container for a keyblock.

  A keyblock is a pre-computed file which contains a signed public key which can
  be used as a link in the chain-of-trust. See:
  http://www.chromium.org/chromium-os/chromiumos-design-docs/verified-boot-data-structures

  Attributes:
    name: name of keyblock
    keydir: location of keyblock
    filename: complete filename of keyblock
  """

  def __init__(self, name, keydir=None, basename=None, data_key=None):
    """Initialize Keyblock.

    Args:
      name: name of keyblock
      keydir: directory that contains keyblock file. Default to data_key.keydir
      basename: basename of keyblock, to which '.keyblock' will be appended.
        defaults to 'name'
      data_key: Key contained inside the keyblock.
    """
    self.name = name

    if keydir is None:
      if data_key is not None and data_key.keydir is not None:
        self.keydir = data_key.keydir
      else:
        raise ValueError("No directory defined")
    else:
      self.keydir = keydir

    self.data_key = data_key

    if basename is None:
      basename = name

    self.filename = os.path.join(self.keydir, basename + '.keyblock')

  def Exists(self):
    """Returns True if keyblock exists on disk."""
    return os.path.exists(self.filename)


class Keyset(object):
  """Store signer keys and keyblocks (think keychain).

  Attributes:
    keys: dict of keypairs, indexed on key's name
    keyblocks: dict of Keyblocks, indexed on keyblock's name
  """
  def __init__(self):
    self.keys = {}
    self.keyblocks = {}

  def Prune(self):
    """Check that all keys and keyblocks exists, else remove them."""
    for k, key in self.keys.items():
      if not key.Exists():
        self.keys.pop(k)

    for k, keyblock in self.keyblocks.items():
      if not keyblock.Exists():
        self.keyblocks.pop(k)

  def AddKey(self, new_key, key_name=None):
    """Add the given key to keyset, using new_key.name if key_name is None."""
    if key_name is None:
      self.keys[new_key.name] = new_key
    else:
      self.keys[key_name] = new_key

  def GetKey(self, key_name):
    """Returns key from key_name. Returns None if not found."""
    return self.keys.get(key_name)

  def KeyExists(self, key_name, require_public=False, require_private=False):
    """Returns if key is in keyset and exists."""
    key = self.GetKey(key_name)

    return key is not None and key.Exists(require_public=require_public,
                                          require_private=require_private)

  def AddKeyblock(self, keyblock, kb_name=None):
    """Add keyblock to keyset indexed by kb_name, use keyblock.name if None."""
    self.keyblocks[kb_name or keyblock.name] = keyblock

  def GetKeyblock(self, keyblock_name):
    """Returns keyblock from keyblock_name. Returns None if not found."""
    return self.keyblocks.get(keyblock_name)

  def KeyblockExists(self, keyblock_name):
    """Returns if the keyblock exists."""
    keyblock = self.GetKeyblock(keyblock_name)
    return keyblock is not None and keyblock.Exists()


def KeysetFromDir(key_dir):
  """Returns a populated keyset generated from given directory

  Note: every public key and keyblock must have an accompanying private key
  """

  # Match any private key file name
  # Ex: firmware_data_key.loem4.vbprivk, kernel_subkey.vbprivk
  re_keypair = re.compile(r'(?P<name>\w+(\.loem(?P<loem>\d+))?)\.vbprivk')

  ks = Keyset()

  for f_name in os.listdir(key_dir):
    key_match = re_keypair.match(f_name)
    if key_match:
      key_name = key_match.group('name')
      #TODO (chingcodes): add version from file
      key = KeyPair(key_name, key_dir, version=1)
      ks.AddKey(key)

      #TODO (chingcodes): loem.ini logic here

      kb = key.CreateKeyblock()
      if kb.Exists():
        ks.AddKeyblock(kb)

  return ks

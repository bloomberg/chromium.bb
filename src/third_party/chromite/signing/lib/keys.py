# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS encryption/decryption key management"""

from __future__ import print_function

import collections
import ConfigParser
import os
import re

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class SignerKeyError(Exception):
  """Raise when there in an error related to keys"""


class SignerKeyMissingError(SignerKeyError):
  """Raise if key is missing from subset"""


class SignerBuildtargetKeyMissingError(SignerKeyMissingError):
  """Raise if a buildtarget-specific key is missing"""


class VersionOverflowError(SignerKeyError):
  """Raise if incrementing a version would overflow 16 bits"""


class KeyPair(object):
  """Container for a key's files.

  A KeyPair contains the information about a particular public/private pair of
  keys, including file names, and version.

  Attributes:
    name: name of keypair
    keydir: location of key files
    version: version of key
    public: public key file complete path
    private: private key file complete path
    keyblock: keyblock file complete path
  """

  # On disk, we have (valid) file names like:
  # - firmware_data_key.vbprivk  -- non-unified build
  # - firmware_data_key.loem1.vbprivk -- unified build
  # - kernel_data_key.vbprivk
  # - kernel_subkey.vbprivk
  # - key_ec_efs.vbprik2 (which pairs with .vbpubk2, instead of .vbpubk)

  # All of the following regular expressions are used to validate names and
  # extensions. From KeyPair's perspective, a name is simply a string of 2 or
  # more alphanumeric characters, possibly containing a single '.' somewhere in
  # the middle, with one of two valid extensions ('.vbprik2' or '.vbprivk').
  _name_re = re.compile(r'\w+\.?\w+$')

  # Valid key file name endings:
  # - .vbprivk (pairs with .vbpubk)
  # - .vbprik2 (pairs with .vbpubk2)
  _priv_ext_re = re.compile(r'\.vbpri(vk|k2)$')
  _pub_ext_re = re.compile(r'\.vbpubk2?$')

  # This is used by ParsePrivateKeyFilename to recognize a valid private key
  # file name.  Keyset initialization only creates KeyPairs for valid private
  # keys that it finds in the directory.
  _priv_filename_re = re.compile(
      r'(?P<name>\w+\.?\w+)(?P<ext>\.vbpri(?:vk|k2))$')

  def __init__(self, name, keydir, version=1,
               pub_ext=None, priv_ext='.vbprivk'):
    """Initialize KeyPair.

    Args:
      name: name of key
      keydir: directory containing key files
      version: version of the key (forced to int)
      pub_ext: file extension used for public key
      priv_ext: file extension used for private key
    """
    self.name = name
    self.keydir = keydir
    self.version = int(version)
    # Use the correct version for pub_ext if they did not specify.
    self._pub_ext = (pub_ext if pub_ext else
                     '.vbpubk' if priv_ext == '.vbprivk' else
                     '.vbpubk2')
    self._priv_ext = priv_ext

    # Validate input parameters.
    if not self._name_re.match(name):
      raise ValueError('Illegal value for name')
    if not self._pub_ext_re.match(self._pub_ext):
      raise ValueError('Illegal value for pub_ext')
    if not self._priv_ext_re.match(priv_ext):
      raise ValueError('Illegal value for priv_ext')

    self.public = os.path.join(keydir, name + self._pub_ext)
    self.private = os.path.join(keydir, name + self._priv_ext)

    # As we create the keyblock name, strip '_data_key' from any name, since
    # this is a common convention.
    keyblock_name = ''.join(name.split('_data_key'))
    self.keyblock = os.path.join(keydir, keyblock_name + '.keyblock')

  def __eq__(self, other):
    return (isinstance(other, KeyPair)
            and self.name == other.name
            and self.version == other.version
            and self.public == other.public
            and self.private == other.private)

  def Copy(self):
    """Return a copy of ourselves."""
    return KeyPair(
        self.name, self.keydir, self.version, self._pub_ext, self._priv_ext)

  @classmethod
  def ParsePrivateKeyFilename(cls, file_name):
    """Extract the name and extension from the filename.

    Args:
      file_name: a filename that may or may not be a private key.  Leading
          directories are ignored.

    Returns:
      None or re.MatchObject with named groups 'name' and 'ext'.
    """
    basename = file_name.split('/')[-1]
    return cls._priv_filename_re.match(basename)

  def Exists(self, require_public=False, require_private=False):
    """Returns True if key exists on disk."""

    has_public = os.path.exists(self.public)
    has_private = os.path.exists(self.private)

    if require_public and not has_public:
      return False
    if require_private and not has_private:
      return False
    return has_public or has_private

  def KeyblockExists(self):
    """Returns if keyblock exist."""
    return os.path.exists(self.keyblock)

  def GetSHA1sum(self):
    """Returns key's sha1sum returns.

    Raises:
      SignerKeyError: If error getting sha1sum from public key
      RunCommandError: if vbutil_key fails
    """
    res = cros_build_lib.RunCommand(['vbutil_key', '--unpack', self.public],
                                    error_code_ok=True)

    # Match line that looks like: 'Key sha1sum: <sha1sum>'.
    match = re.search(r'Key sha1sum: +(\w+)', res.output)
    if match:
      return match.group(1)
    else:
      raise SignerKeyError('Unable to get sha1sum for %s' % self.public)

class KeyVersions(object):
  """Manage key.versions file

  Attributes:
    saved: True if the file on disk matches the contents of the instance.  None
        of the methods save the instance to disk automatically.
  """

  def __init__(self, filename):
    self._versions = {}
    self._path = filename
    if os.path.exists(filename):
      self.saved = True
      for line in open(filename, "r").readlines():
        if line.find('=') > 0:
          k, v = line.strip().split('=')
          self._versions[k] = int(v)
    else:
      self.saved = False
      self._versions = {
          'firmware_key_version': 1,
          'firmware_version': 1,
          'kernel_key_version': 1,
          'kernel_version': 1,
      }
    # Caller is resonsible for calling Save()

  def _KeyName(self, key):
    """return the correct name to use when looking up version."""
    # We want to be idempotent.
    if key.endswith('_version'):
      key = key[:-8]
    # strip anything after a '.', since those are buildtarget names.
    key = key.split('.')[0]
    if key.endswith('_data_key'):
      key = key[:-9]
    return key + '_version'

  def Get(self, key):
    """Get a key's version.  Caller is responsible for calling Save()."""
    key = self._KeyName(key)
    return self._versions.get(key, 1)

  def Set(self, key, version):
    """Set a key's version.  Caller is responsible for calling Save()."""
    key = self._KeyName(key)
    self._versions[key] = int(version)
    self.saved = False
    # Caller is resonsible for calling Save()

  def Increment(self, key):
    """Increment key's version.  Caller is responsible for calling Save().

    Args:
      key: name of the key.

    Returns:
      Incremented version.

    Raises:
      VersionOverflowError: version is 16 bit, and incrementing would cause
          overflow.
    """
    key = self._KeyName(key)
    if self._versions[key] == 0xffff:
      raise VersionOverflowError('%s version overflow' % key)
    self._versions[key] += 1
    self.saved = False
    return self._versions[key]
    # Caller is resonsible for calling Save()

  def Save(self):
    """Save KeyVersions to disk."""
    lines = ['%s=%d' % (k, v) for k, v in sorted(self._versions.items())]
    contents = '\n'.join(lines) + '\n'
    osutils.WriteFile(self._path, contents)
    self.saved = True


class Keyset(object):
  """Store signer keys and keyblocks (think keychain).

  A Keyset is the collection of KeyPairs needed to work with a specific Build
  Target image.
  This includes both keys shared by the Build (self.keys):
    - installer_kernel_data_key
    - kernel_data_key
    - kernel_subkey
    - recovery
    - recovery_kernel_data_key
  as well as Build Target specific keys (self._buildtarget_keys):
    - root_key
    - firmware_data_key

  Attributes:
    keys: dict of keypairs, indexed on key's name
    buildtarget_map: dict of buildtarget alias (e.g., 'loem1') to use for each
        buildtarget name (e.g. 'ACME').  Keys in the table are both buildtarget
        names and buildtarget aliases, so that
        buildtarget_map[buildtarget_map[buildtarget_name]] works.
  """

  # If we have a buildtarget name, it is of the form 'name\.buildtarget', so we
  # will simply split('.') the name to get the components.
  # If this is a unified buildtarget, then there are per-buildtarget keys, and
  # self.buildtarget_key_prefixes will be set to this.
  _per_buildtarget_key_names = set(('firmware_data_key', 'root_key'))

  def __init__(self, key_dir=None):
    """Initialize the Keyset from key_dir, if given.

    Args:
      key_dir: directory from which to load Keyset.  [default=None]

    Note: every public key and keyblock must have an accompanying private key
    """
    self.keys = {}
    self.key_dir = key_dir
    self.buildtarget_map = {}
    self._buildtarget_key_prefixes = set()
    self._buildtarget_keys = collections.defaultdict(dict)
    if key_dir and os.path.exists(key_dir):
      # Get all buildtarget aliases.  The legacy code base refers to
      # 'buildtarget' as 'loem'. We need to support the on-disk structures
      # which have a table of 'XX = ALIAS', with the implied name 'loemXX'.
      loem_config_filename = os.path.join(key_dir, 'loem.ini')
      if os.path.exists(loem_config_filename):
        logging.info("Reading loem.ini file")
        loem_config = ConfigParser.ConfigParser()
        if loem_config.read(loem_config_filename):
          if loem_config.has_section('loem'):
            self._buildtarget_key_prefixes = self._per_buildtarget_key_names
            for idx, loem in loem_config.items('loem'):
              alias = 'loem' + idx
              logging.debug('Adding loem alias %s %s', loem, alias)
              self.buildtarget_map[loem] = alias
              # We also want loemXX to point to loemXX, since our callers tend
              # to use both name and alias interchangably.
              # TODO(lamontjones) evaluate whether or not we should force it to
              # be indexed by only name, instead of both.
              self.buildtarget_map[alias] = alias
        else:
          logging.warning("Error reading loem.ini file")

      versions_filename = os.path.join(key_dir, 'key.versions')
      self._versions = KeyVersions(versions_filename)

      # Match any private key file name
      # Ex: firmware_data_key.loem4.vbprivk, kernel_subkey.vbprivk
      for f_name in os.listdir(key_dir):
        match = KeyPair.ParsePrivateKeyFilename(f_name)
        if match:
          key_name = match.group('name')
          if key_name not in self.keys:
            logging.debug('Found new key %s', key_name)
            key = KeyPair(
                key_name,
                key_dir,
                version=self._versions.Get(key_name),
                priv_ext=match.group('ext'))
            # AddKey will detect whether or not this is a buildtarget-specific
            # key and do the right thing.
            self.AddKey(key)

  def __eq__(self, other):
    return (isinstance(other, Keyset)
            and self.buildtarget_map == other.buildtarget_map
            and self.keys == other.keys)

  def Prune(self):
    """Check that all keys exists, else remove them."""
    for k, key in self.keys.items():
      if not key.Exists():
        self.keys.pop(k)
    for buildtarget, keys in self._buildtarget_keys.items():
      for k, key in keys.items():
        if not key.Exists():
          self._buildtarget_keys[buildtarget].pop(k)

  def AddKey(self, key):
    """Add key to Keyset.

    Args:
      key: The KeyPair to add.  key.name is checked to see if it is
          buildtarget-specific, and the correct group is used.
    """
    if '.' in key.name:
      key_name, buildtarget_name = key.name.split('.')
      # Some of the legacy keyfiles have .vN.vprivk suffixes, even though they
      # are not buildtarget keys. (They are backup keys for older versions of
      # the key.)  Restricting the buildtarget_keys to those in
      # _buildtarget_key_prefixes helps with that.
      if key_name in self._buildtarget_key_prefixes:
        logging.debug('Found buildtarget %s.%s', key_name, buildtarget_name)
        self.AddBuildtargetKey(key_name, buildtarget_name, key)
        return
    self.keys[key.name] = key

  def AddBuildtargetKey(self, key_name, buildtarget_alias, key):
    """Attach the buildtarget-specific key to the base key."""
    # _buildtarget_keys['loem2']['root_key'] = KeyPair('root_key.loem2', ...)
    self._buildtarget_keys[buildtarget_alias][key_name] = key

  def KeyExists(self, key_name, require_public=False, require_private=False):
    """Returns if key is in Keyset and exists.

    If this Keyset has buildtarget-specific keys, then buildtarget-specific keys
    will only be found if GetBuildKeyset() has been called to get the
    buildtarget-specific Keyset.
    """
    return (key_name in self.keys and
            self.keys[key_name].Exists(require_public=require_public,
                                       require_private=require_private))

  def KeyblockExists(self, key_name):
    """Returns if keyblock exists

    If this Keyset has buildtarget-specific keys, then keyblocks for
    buildtarget-specific keys will only be found if GetBuildKeyset() has
    been called to get the buildtarget-specific Keyset.
    """
    return (key_name in self.keys and
            self.keys[key_name].KeyblockExists())

  def GetBuildtargetKeys(self, key_name):
    """Get buildtarget-specific keys by keyname.

    Args:
      key_name: name of buildtarget-specific key.  e.g., 'root_key'

    Returns:
      dict of buildtarget_alias: key
    """
    ret = {}
    for k, v in self._buildtarget_keys.items():
      if key_name in v:
        ret[k] = v[key_name]
    if key_name in self.keys:
      ret[key_name] = self.keys[key_name]
    return ret

  def GetBuildKeyset(self, buildtarget_name):
    """Returns new Keyset containing keys based on the buildtarget_name given.

    The following keys are included:
    * This buildtarget's buildtarget-specific keys
    * Any non-buildtarget-specific keys.

    Args:
      buildtarget_name: either the buildtarget name (e.g., 'acme') or alias
          (e.g., 'loem1').

    Raises SignerBuildtargetKeyMissingError if subkey not found
    """
    ks = Keyset()

    found = False

    # Use alias if exists
    buildtarget_alias = self.buildtarget_map.get(buildtarget_name, '')

    for key in self.keys.values():
      ks.AddKey(key)
    for key in self._buildtarget_keys[buildtarget_alias].values():
      found = True
      ks.AddKey(key)
      # Also add the key as its base name.
      key = key.Copy()
      key.name = key.name.split('.')[0]
      ks.AddKey(key)

    if not found:
      raise SignerBuildtargetKeyMissingError(
          "Unable to find %s", buildtarget_name)

    return ks

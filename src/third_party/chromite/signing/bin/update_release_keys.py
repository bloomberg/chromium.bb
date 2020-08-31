# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Import keys from legacy signer config."""

from __future__ import print_function

import os

from six.moves import configparser
import yaml # pylint: disable=import-error

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.signing.lib import keys


METADATA_VERSION = 0
PROD_DIR = '/cros'
BASE_DIR = os.path.join(PROD_DIR, 'v2-keys')
AU_KEYDIR = 'update-payload-key'


class Error(Exception):
  """Base error class."""

class KeyimportError(Error):
  """Key Import Errors."""


class KeyringData(object):
  """A collection of keyset information."""

  def __init__(self, prod_dir, base_dir, options, signer_config):
    self.prod_dir = prod_dir
    self.base_dir = base_dir
    self.options = options
    self.signer_config = signer_config
    self.prod_keys = os.path.join(prod_dir, 'keys')
    self.public = os.path.join(base_dir, 'public')
    self.private = '../private'
    self.configs = '../config'
    self.contents_yaml = os.path.join(self.base_dir, 'contents.yaml')
    self.keysets = self._ReadRepoYaml()

  def _ReadRepoYaml(self):
    """Read the directory of active keysets in the repo."""
    default = {'metadata-version': METADATA_VERSION}
    if os.path.exists(self.contents_yaml):
      with open(self.contents_yaml) as fp:
        ret = yaml.load(fp)
      if not ret or 'metadata-version' not in ret:
        logging.error('%s: no metadata-version.', self.contents_yaml)
        raise KeyimportError('metadata-version missing.')
      if ret['metadata-version'] != METADATA_VERSION:
        logging.error('%s: metadata-version %s', self.contents_yaml,
                      ret['metadata-version'])
        raise KeyimportError('metadata-version bad.')
      return ret
    else:
      return default

  def WriteRepoYaml(self):
    """Write repo.yaml."""
    self._WriteConfig(self.contents_yaml, self.keysets)

  def _WriteConfig(self, filename, config):
    """Write the config

    Args:
      filename: path to file.
      config: keyset dictionary.
    """
    content = yaml.dump(config, default_flow_style=False)
    osutils.WriteFile(filename, content)

  def ImportKeyset(self, setname, directory):
    """Import a Keyset.

    Args:
      setname: Friendly setname (e.g., sarien-mp-v3)
      directory: Directory where keyset is stored. (e.g., SarienMPKeys-v3)

    Returns:
      True if all files were processed.
    """
    config_file = os.path.join(self.configs, '%s.yaml' % setname)
    if os.path.exists(config_file):
      if self.options.new_only:
        logging.debug('Skipping existing keyset %s (%s).', setname, directory)
        return False
      if self.options.update:
        osutils.SafeUnlink(config_file)
      else:
        raise KeyimportError('%s.yaml already exists' % setname)

    logging.info('Processing keyset %s (%s):', setname, directory)

    set_config = {
        'name': setname,
        'metadata-version': METADATA_VERSION,
        'directory': directory,
    }

    if setname.startswith('test-keys-'):
      group = 'test-keysets'
    elif '-premp' in setname:
      group = 'premp-keysets'
    elif '-mp' in setname:
      group = 'mp-keysets'
    # These two are special case.
    elif setname in ('update_signer', 'oci_containers_signer'):
      group = 'mp-keysets'
    else:
      # This will cause the schema validation to fail, notifying the user.
      logging.warning('Bad keyset setname %s', setname)
      group = 'bad-keysets'
    self.keysets.setdefault(group, []).append(setname)
    return self._WriteConfig(config_file, set_config)


def ParseArgs(argv):
  """Parse the commandline arguments."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-P', '--prefix', type='path', default='/')
  parser.add_argument('-d', '--directory', default=BASE_DIR)
  parser.add_argument('-D', '--discover', action='store_true',
                      help='Discover keysets')
  parser.add_argument('-s', '-p', '--production', default=PROD_DIR)
  parser.add_argument('-N', '--new-only', action='store_true',
                      help='Only process new keysets')
  parser.add_argument('-u', '--update', action='store_true')
  parser.add_argument('keysets', nargs='*')

  options = parser.parse_args(argv)
  options.Freeze()
  return options


def ParseSignerConfig(prod_path):
  """Return the parsed signer config.

  Args:
    prod_path: Path to production checkout.  Typically '/cros'.

  Returns:
    Parsed signer config.
  """
  config = configparser.ConfigParser()
  config_path = os.path.join(prod_path, 'signer/configs/cros_common.config')
  if not os.path.exists(config_path):
    logging.warning('%s not found, using production config.', config_path)
    config_path = '/cros/signer/configs/cros_common.config'
  config.read(config_path)
  return config


def DiscoverKeysets(keysets_dir):
  """Discover keysets.

  Args:
    keysets_dir: directory where the keysets live.  Typically /cros/keys.

  Returns:
    A sorted list of (setname: directory) tuples.
  """
  _, dirs, _ = next(os.walk(keysets_dir))
  ret = {}
  for src in sorted(dirs):
    path = os.path.join(keysets_dir, src)
    keyset = keys.Keyset(path)
    if keyset.name != 'unknown':
      if keyset.name in ret:
        logging.warning('Ignoring %s because name is duplicate of %s',
                        src, ret[keyset.name])
      else:
        logging.info('Discovered %s in %s', keyset.name, src)
        ret[keyset.name] = src
  return sorted(ret.items())


def main(argv):
  """Import the keys."""
  options = ParseArgs(argv)
  # Always prepend the prefix directory.
  production = os.path.join(
      options.prefix, options.production.lstrip('/'))
  directory = os.path.join(
      options.prefix, options.directory.lstrip('/'))
  config = ParseSignerConfig(production)

  keydata = KeyringData(production, directory, options, config)

  if osutils.SafeMakedirs(keydata.public):
    os.chdir(keydata.public)
    osutils.SafeMakedirs(keydata.private)
    osutils.SafeMakedirs(keydata.configs)
  else:
    os.chdir(keydata.public)

  if not options.discover and config.has_section('keysets'):
    config_keysets = config.items('keysets')
  else:
    logging.notice('Discovering keysets in %s' % options.production)
    config_keysets = DiscoverKeysets(os.path.join(options.production, 'keys'))

  # Handle autoupdate keysets first.
  if not options.keysets:
    keysets = config_keysets
  else:
    # Allow '<setname>:<directory>' as well as '<setname>'.
    keysets = []
    keyset_dict = dict(config_keysets)
    for opt in options.keysets:
      if ':' in opt:
        keysets.append(opt.split(':', 1))
      elif opt in keyset_dict:
        keysets.append((opt, keyset_dict[opt]))

  # If the auto-update keyset is present, process it first.
  au_keysets = [x for x in keysets if x[1] == AU_KEYDIR]
  for keyset in au_keysets:
    keysets.remove(keyset)
  keysets = au_keysets + keysets
  logging.notice('Processing: %s', ' '.join(x[0] for x in keysets))

  for setname, directory in keysets:
    # TODO(lamontjones): consider flattening the namespace by combining multiple
    # directories/sets into a common directory.  That would require adding
    # version numbers to colliding files.
    keydata.ImportKeyset(setname, directory)

  keydata.WriteRepoYaml()
  logging.notice('Done')

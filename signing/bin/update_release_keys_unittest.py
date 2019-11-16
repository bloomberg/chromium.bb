# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for update_release_keys."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

import mock

from six.moves import configparser
from six.moves import StringIO

from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.signing.bin import update_release_keys


# pylint: disable=protected-access

def MockConfig(config_str):
  ret = configparser.ConfigParser()
  ret.readfp(StringIO.StringIO(config_str))
  return ret


class TestImportKeyset(cros_test_lib.MockTempDirTestCase):
  """Tests for ImportKeyset."""

  def setUp(self):
    self.prod = os.path.join(self.tempdir, 'prod')
    os.makedirs(os.path.join(self.prod, 'keys'))
    self.base = os.path.join(self.prod, 'v2-keys')
    os.makedirs(self.base)
    for dname in 'public', 'config':
      os.mkdir(os.path.join(self.base, dname))
    conf_path = os.path.join(
        self.prod, 'signer', 'configs', 'cros_common.config')
    os.makedirs(os.path.dirname(conf_path))
    with open(conf_path, 'w') as fp:
      fp.write('[keysets]\nfoo-mp-v3 = FooMPKeys-v3\n')
    self.config = update_release_keys.ParseSignerConfig(self.prod)
    self.argv = ['-d', self.base]
    options = update_release_keys.ParseArgs(self.argv)
    self.keydata = update_release_keys.KeyringData(
        self.prod, self.base, options, self.config)
    os.chdir(self.keydata.public)
    self.wc = self.PatchObject(self.keydata, '_WriteConfig')
    self.logdebug = self.PatchObject(logging, 'debug')
    self.loginfo = self.PatchObject(logging, 'info')
    self.lognotice = self.PatchObject(logging, 'notice')
    self.logwarning = self.PatchObject(logging, 'warning')
    self.logerror = self.PatchObject(logging, 'error')

  def test_ImportKeys(self):
    """Test normal case."""
    data = (
        ('update_signer', 'update-payload-key', 'mp'),
        ('oci_containers_signer', 'oci-containers-key', 'mp'),
        ('foo-xp', 'FooXPKeys', 'bad'),
        ('foo-mp', 'FooMPKeys', 'mp'),
        ('foo-mp-v3', 'FooMPKeys-v3', 'mp'),
        ('foo-premp', 'FooPreMPKeys', 'premp'),
        ('test-keys-blah', 'test-keys-blah', 'test'),
    )

    kd = self.keydata
    expected_keysets = {'metadata-version': 0}
    expected_calls = []
    for sname, dname, settype in data:
      kd.ImportKeyset(sname, dname)
      expected_keysets.setdefault('%s-keysets' % settype, []).append(sname)
      expected_calls.append(
          mock.call(
              os.path.join(kd.configs, '%s.yaml' % sname),
              {'name': sname, 'metadata-version': 0, 'directory': dname}))
    self.assertDictEqual(expected_keysets, kd.keysets)
    self.assertEqual(expected_calls, self.wc.call_args_list)

  def test_ImportKeysChecksExists(self):
    """Test that existing configs are properly handled."""
    kd = self.keydata
    osutils.WriteFile(os.path.join(kd.configs, 'foo-mp.yaml'),
                      'bad: 0', makedirs=True)
    self.assertRaises(update_release_keys.KeyimportError, kd.ImportKeyset,
                      'foo-mp', 'FooMPKeys')

    kd.options = update_release_keys.ParseArgs(self.argv + ['--new-only'])
    kd.ImportKeyset('foo-mp', 'FooMPKeys')
    self.assertEqual({'metadata-version': 0}, kd.keysets)
    self.logdebug.assert_called_once()

  def test_ImportKeysUpdatesExisting(self):
    """Test that existing configs are properly handled."""
    kd = self.keydata
    conf_file = os.path.join(kd.configs, 'foo-mp.yaml')
    osutils.WriteFile(conf_file, 'bad: 0', makedirs=True)
    unl = self.PatchObject(osutils, 'SafeUnlink')
    kd.options = update_release_keys.ParseArgs(self.argv + ['--update'])
    kd.ImportKeyset('foo-mp', 'FooMPKeys')
    unl.assert_called_once_with(conf_file)


class TestRepoYaml(cros_test_lib.MockTempDirTestCase):
  """Tests for Read/Write RepoYaml."""

  def setUp(self):
    self.prod = os.path.join(self.tempdir, 'prod')
    os.makedirs(os.path.join(self.prod, 'keys'))
    self.base = os.path.join(self.prod, 'v2-keys')
    os.makedirs(self.base)
    for dname in 'public', 'config':
      os.mkdir(os.path.join(self.base, dname))
    conf_path = os.path.join(
        self.prod, 'signer', 'configs', 'cros_common.config')
    os.makedirs(os.path.dirname(conf_path))
    with open(conf_path, 'w') as fp:
      fp.write('[keysets]\nfoo-mp-v3 = FooMPKeys-v3\n')
    self.config = update_release_keys.ParseSignerConfig(self.prod)
    self.argv = ['-d', self.base]
    self.logerror = self.PatchObject(logging, 'error')

  def test_ReadsRepoYaml(self):
    """Test missing file case."""
    osutils.WriteFile(os.path.join(self.base, 'contents.yaml'),
                      'metadata-version: 0\nmp-keysets: [foo-mp]\n')
    options = update_release_keys.ParseArgs(self.argv)
    kd = update_release_keys.KeyringData(
        self.prod, self.base, options, self.config)
    self.assertDictEqual({'metadata-version': 0, 'mp-keysets': ['foo-mp']},
                         kd.keysets)

  def test_ReadMissingRepoYaml(self):
    """Test missing file case."""
    options = update_release_keys.ParseArgs(self.argv)
    kd = update_release_keys.KeyringData(
        self.prod, self.base, options, self.config)
    self.assertDictEqual({'metadata-version': 0}, kd.keysets)

  def test_ReadAssertsProperly(self):
    """Test bad file case."""
    osutils.WriteFile(os.path.join(self.base, 'contents.yaml'), '\n')
    options = update_release_keys.ParseArgs(self.argv)
    self.assertRaises(update_release_keys.KeyimportError,
                      update_release_keys.KeyringData,
                      self.prod, self.base, options, self.config)
    osutils.WriteFile(os.path.join(self.base, 'contents.yaml'),
                      'metadata-version: 9\n')
    options = update_release_keys.ParseArgs(self.argv)
    self.assertRaises(update_release_keys.KeyimportError,
                      update_release_keys.KeyringData,
                      self.prod, self.base, options, self.config)

  def test_WriteRepoYaml(self):
    """Test WriteRepoYaml and _WriteConfig."""
    options = update_release_keys.ParseArgs(self.argv)
    kd = update_release_keys.KeyringData(
        self.prod, self.base, options, self.config)
    self.assertEqual(kd.contents_yaml, os.path.join(self.base, 'contents.yaml'))
    kd.keysets = {'metadata-version': 0, 'mp-keysets': ['foo-mp']}
    kd.WriteRepoYaml()
    self.assertEqual(osutils.ReadFile(kd.contents_yaml),
                     'metadata-version: 0\nmp-keysets:\n- foo-mp\n')

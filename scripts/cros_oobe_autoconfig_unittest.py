# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cros_oobe_autoconfig.py"""

from __future__ import print_function

import json
import os
import pwd

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cros_oobe_autoconfig


_TEST_CLI_PARAMETERS = (
    'image.bin', '--x-demo-mode', '--x-network-onc', '{}',
    '--x-network-auto-connect', '--x-eula-send-statistics',
    '--x-eula-auto-accept', '--x-update-skip', '--x-wizard-auto-enroll')
_TEST_CONFIG_JSON = {
    'demo-mode': True,
    'network-onc': '{}',
    'network-auto-connect': True,
    'eula-send-statistics': True,
    'eula-auto-accept': True,
    'update-skip': True,
    'wizard-auto-enroll': True
}
_IMAGE_SIZE = 4 * 1024 * 1024
_BLOCK_SIZE = 4096
_SECTOR_SIZE = 512
_STATEFUL_SIZE = _IMAGE_SIZE / 2
_STATEFUL_OFFSET = 120 * _SECTOR_SIZE


class PrepareImageTests(cros_test_lib.MockTempDirTestCase):
  """Tests for PrepareImage()"""

  def setUp(self):
    """Create a small test disk image for testing."""
    self.image = os.path.join(self.tempdir, 'image.bin')
    state = os.path.join(self.tempdir, 'state.bin')

    # Allocate space for the disk image and stateful partition.
    osutils.AllocateFile(self.image, _IMAGE_SIZE)
    osutils.AllocateFile(state, _STATEFUL_SIZE)

    commands = (
        # Format the stateful image as ext4.
        ['/sbin/mkfs.ext4', state],
        # Create the GPT headers and entry for the stateful partition.
        ['cgpt', 'create', self.image],
        ['cgpt', 'add', self.image, '-t', 'data', '-l',
         constants.CROS_PART_STATEFUL, '-b', _STATEFUL_OFFSET / _SECTOR_SIZE,
         '-s', _STATEFUL_SIZE / _SECTOR_SIZE, '-i', 1],
        # Copy the stateful partition into the GPT image.
        ['dd', 'if=%s' % state, 'of=%s' % self.image, 'conv=notrunc', 'bs=4K',
         'seek=%d' % (_STATEFUL_OFFSET / _BLOCK_SIZE),
         'count=%s' % (_STATEFUL_SIZE / _BLOCK_SIZE)])
    for cmd in commands:
      cros_build_lib.RunCommand(map(str, cmd), quiet=True)

    # Run the preparation script on the image.
    cros_oobe_autoconfig.main([self.image] + list(_TEST_CLI_PARAMETERS)[1:])

    # Mount the image's stateful partition for inspection.
    self.mount_tmp = os.path.join(self.tempdir, 'mount')
    osutils.SafeMakedirs(self.mount_tmp)
    self.mount_ctx = osutils.MountImageContext(self.image, self.mount_tmp,
                                               (1,), ('rw',))
    self.mount = os.path.join(self.mount_tmp,
                              'dir-%s' % constants.CROS_PART_STATEFUL)

    self.oobe_autoconf_path = os.path.join(self.mount, 'unencrypted',
                                           'oobe_auto_config')
    self.config_path = os.path.join(self.oobe_autoconf_path, 'config.json')

  def testChronosOwned(self):
    """Test that the OOBE autoconfig directory is owned by chronos."""
    with self.mount_ctx:
      chronos_uid = pwd.getpwnam('chronos').pw_uid
      self.assertExists(self.oobe_autoconf_path)
      self.assertEqual(os.stat(self.config_path).st_uid, chronos_uid)

  def testConfigContents(self):
    """Test that the config JSON matches the correct data."""
    with self.mount_ctx, open(self.config_path) as conf:
      data = json.load(conf)
      self.assertEqual(data, _TEST_CONFIG_JSON)


class GetConfigContentTests(cros_test_lib.MockTestCase):
  """Tests for GetConfigContent()"""

  def testBasic(self):
    """Test that config is generated correctly with all options."""
    opts = cros_oobe_autoconfig.ParseArguments(_TEST_CLI_PARAMETERS)
    conf = cros_oobe_autoconfig.GetConfigContent(opts)
    self.assertEqual(json.loads(conf), _TEST_CONFIG_JSON)

  def testUnspecified(self):
    """Test that config is generated correctly with some options missing."""
    cli = list(_TEST_CLI_PARAMETERS)
    cli.remove('--x-update-skip')
    expected = dict(_TEST_CONFIG_JSON)
    expected['update-skip'] = False

    opts = cros_oobe_autoconfig.ParseArguments(cli)
    conf = cros_oobe_autoconfig.GetConfigContent(opts)
    self.assertEqual(json.loads(conf), expected)


class MainTests(cros_test_lib.MockTestCase):
  """Tests for main()"""

  def setUp(self):
    self.PatchObject(cros_oobe_autoconfig, 'PrepareImage')

  def testBasic(self):
    """Simple smoke test"""
    cros_oobe_autoconfig.main(_TEST_CLI_PARAMETERS)

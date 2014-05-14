#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cbuildbot_view_config."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.scripts import cbuildbot_view_config


class JsonDumpTest(cros_test_lib.OutputTestCase):
  """Test the json dumping functionality of cbuildbot_view_config."""

  def setUp(self):
    bin_name = os.path.basename(__file__).rstrip('_unittest.py')
    self.bin_path = os.path.join(constants.CHROMITE_BIN_DIR, bin_name)

  def testJSONDumpLoadable(self):
    """Make sure config export functionality works."""
    for args in (['--dump'], ['--dump', '--pretty']):
      with self.OutputCapturer() as output:
        cbuildbot_view_config.main(args)

      configs = json.loads(output.GetStdout())
      self.assertFalse(not configs)

  def testJSONBuildbotDumpHasOrder(self):
    """Make sure config export functionality works."""
    args = ['--dump', '--for-buildbot']
    with self.OutputCapturer() as output:
      cbuildbot_view_config.main(args)

    configs = json.loads(output.GetStdout())
    for cfg in configs.itervalues():
      self.assertTrue(cfg['display_position'] is not None)

    self.assertFalse(not configs)


class JsonCompareTest(cros_test_lib.TempDirTestCase,
                      cros_test_lib.OutputTestCase):
  """Test the json comparing functionality of cbuildbot_view_config."""

  TARGET = 'x86-mario-paladin'
  DUMPFILE = 'dump.json'

  def _DumpTarget(self):
    """Run cbuildbot_view_config.main with --dump to a temp file."""
    path = os.path.join(self.tempdir, self.DUMPFILE)

    # Call with --dump, capture output and save to path.
    argv = ['--dump', self.TARGET]
    with self.OutputCapturer() as dump_output:
      self.assertEqual(0, cbuildbot_view_config.main(argv))

    with open(path, 'w') as f:
      f.write(dump_output.GetStdout())

    return path

  def testCompareSame(self):
    """Run --compare with no config changes."""
    path = self._DumpTarget()

    # Run with --compare now.
    argv = ['--compare', path, self.TARGET]
    with self.OutputCapturer() as compare1_output:
      self.assertEqual(0, cbuildbot_view_config.main(argv))

    # Expect no output.
    self.assertFalse(compare1_output.GetStdout().strip())
    self.assertFalse(compare1_output.GetStderr().strip())

  def testCompareDifferent(self):
    """Run --compare with config changes."""
    path = self._DumpTarget()

    # Tweak the config value for TARGET and run comparison.
    config = cbuildbot_view_config.cbuildbot_config.config
    orig_name = config[self.TARGET]['name']
    try:
      config[self.TARGET]['name'] = 'FOO'

      argv = ['--compare', path, self.TARGET]
      with self.OutputCapturer() as compare2_output:
        self.assertEqual(0, cbuildbot_view_config.main(argv))

      # Expect output on stdout that mentions FOO.
      self.assertTrue(compare2_output.GetStdout().strip())
      self.assertFalse(compare2_output.GetStderr().strip())
      self.assertTrue('FOO' in compare2_output.GetStdout())

    finally:
      config[self.TARGET]['name'] = orig_name


if __name__ == '__main__':
  cros_test_lib.main()

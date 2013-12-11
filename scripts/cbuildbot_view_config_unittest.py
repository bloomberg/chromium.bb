#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cbuildbot_view_config."""

import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_test_lib


class JsonDumpTest(cros_test_lib.TestCase):
  """Test the json dumping functionality of cbuildbot_view_config."""

  def setUp(self):
    bin_name = os.path.basename(__file__).rstrip('_unittest.py')
    self.bin_path = os.path.join(constants.CHROMITE_BIN_DIR, bin_name)

  # TODO(build): Add test for compare functionality
  def testJSONDumpLoadable(self):
    """Make sure config export functionality works."""
    for args in (['--dump'], ['--dump', '--pretty']):
      # TODO(build): Leverage cbuildbot_view_config.main() directly and
      # capture output with cros_test_lib.OutputTestCase (or similar).
      output = subprocess.Popen([self.bin_path] + args,
                                stdout=subprocess.PIPE).communicate()[0]
      configs = json.loads(output)
      self.assertFalse(not configs)

  def testJSONBuildbotDumpHasOrder(self):
    """Make sure config export functionality works."""
    output = subprocess.Popen([self.bin_path, '--dump',
                               '--for-buildbot'],
                              stdout=subprocess.PIPE).communicate()[0]
    configs = json.loads(output)
    for cfg in configs.itervalues():
      self.assertTrue(cfg['display_position'] is not None)

    self.assertFalse(not configs)


if __name__ == '__main__':
  cros_test_lib.main()

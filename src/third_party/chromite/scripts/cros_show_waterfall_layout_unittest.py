# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cros_show_waterfall_layout."""

from __future__ import print_function

import os
import sys

from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.scripts import cros_show_waterfall_layout


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# pylint: disable=protected-access


class DumpTest(cros_test_lib.OutputTestCase):
  """Test the dumping functionality of cros_show_waterfall_layout."""

  def setUp(self):
    bin_name = os.path.basename(__file__).rstrip('_unittest.py')
    self.bin_path = os.path.join(constants.CHROMITE_BIN_DIR, bin_name)

  def testTextDump(self):
    """Make sure text dumping is capable of being produced."""
    with self.OutputCapturer() as output:
      cros_show_waterfall_layout.main([])
    self.assertFalse(not output.GetStdout())

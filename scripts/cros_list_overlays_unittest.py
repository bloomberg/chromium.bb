#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cros_list_overlays.py"""

from __future__ import print_function

import logging
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.cbuildbot import portage_utilities
from chromite.lib import cros_test_lib
from chromite.scripts import cros_list_overlays


class ListOverlaysTest(cros_test_lib.MockTestCase):
  """Tests for main()"""

  def setUp(self):
    self.pfind_mock = self.PatchObject(portage_utilities, 'FindPrimaryOverlay')
    self.find_mock = self.PatchObject(portage_utilities, 'FindOverlays')

  def testSmoke(self):
    """Basic sanity check"""
    cros_list_overlays.main([])

  def testPrimary(self):
    """Basic primary check"""
    cros_list_overlays.main(['--primary_only', '--board', 'foo'])


if __name__ == '__main__':
  cros_test_lib.main(level=logging.INFO)

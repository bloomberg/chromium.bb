# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import shutil
import tempfile
import unittest

from profile_creators.history_profile_extender import HistoryProfileExtender
from telemetry.core import util
from telemetry import decorators
from telemetry.unittest_util import options_for_unittests

util.AddDirToPythonPath(util.GetTelemetryDir(), 'third_party', 'mock')
import mock  # pylint: disable=import-error


# Testing private method.
# pylint: disable=protected-access
class HistoryProfileExtenderTest(unittest.TestCase):
  # The profile extender does not work on Android or ChromeOS.
  @decorators.Disabled('android', 'chromeos')
  def testFullFunctionality(self):
    options = options_for_unittests.GetCopy()
    options.output_profile_path = tempfile.mkdtemp()
    extender = HistoryProfileExtender(options)

    # Stop the extender at the earliest possible opportunity.
    extender.ShouldExitAfterBatchNavigation = mock.MagicMock(return_value=True)
    # Normally, the number of tabs depends on the number of cores. Use a
    # static, small number to increase the speed of the test.
    extender._NUM_TABS = 3

    try:
      extender.Run()
      self.assertEquals(extender.profile_path, options.output_profile_path)
      self.assertTrue(os.path.exists(extender.profile_path))
      history_db_path = os.path.join(extender.profile_path, "Default",
          "History")
      stat_info = os.stat(history_db_path)
      self.assertGreater(stat_info.st_size, 1000)
    finally:
      shutil.rmtree(options.output_profile_path)

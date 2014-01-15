# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import loading_timeline
from telemetry.core import wpr_modes
from telemetry.page import page_measurement_unittest_base
from telemetry.unittest import options_for_unittests

class LoadingTimelineUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testLoadingTimelineBasic(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = loading_timeline.LoadingTimeline()
    timeline_options = self._options
    results = self.RunMeasurement(measurement, ps, options = timeline_options)
    self.assertEquals(0, len(results.failures))

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import thread_times
from telemetry.core import wpr_modes
from telemetry.page import page_measurement_unittest_base
from telemetry.unittest import options_for_unittests
from metrics import timeline

class ThreadTimesUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testBasic(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = thread_times.ThreadTimes()
    timeline_options = self._options
    results = self.RunMeasurement(measurement, ps, options = timeline_options)
    self.assertEquals(0, len(results.failures))

    for category in timeline.TimelineThreadCategories.values():
      cpu_time_name = timeline.ThreadCpuTimeResultName(category)
      cpu_time = results.FindAllPageSpecificValuesNamed(cpu_time_name)
      self.assertEquals(len(cpu_time), 1)

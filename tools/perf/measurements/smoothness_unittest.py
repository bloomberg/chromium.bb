# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import smoothness
from telemetry.core import wpr_modes
from telemetry.page import page_measurement_unittest_base
from telemetry.unittest import options_for_unittests
from metrics import timeline

class SmoothnessUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
  """Smoke test for smoothness measurement

     Runs smoothness measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testSmoothnessWithSmoothnessMetric(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    frame_times = results.FindAllPageSpecificValuesNamed('frame_times')
    self.assertEquals(len(frame_times), 1)
    self.assertGreater(frame_times[0].GetRepresentativeNumber(), 0)

    mean_frame_time = results.FindAllPageSpecificValuesNamed('mean_frame_time')
    self.assertEquals(len(mean_frame_time), 1)
    self.assertGreater(mean_frame_time[0].GetRepresentativeNumber(), 0)

    jank = results.FindAllPageSpecificValuesNamed('jank')
    self.assertEquals(len(jank), 1)
    self.assertGreater(jank[0].GetRepresentativeNumber(), 0)

    mostly_smooth = results.FindAllPageSpecificValuesNamed('mostly_smooth')
    self.assertEquals(len(mostly_smooth), 1)
    self.assertGreaterEqual(mostly_smooth[0].GetRepresentativeNumber(), 0)


  def testSmoothnessWithTimelineMetric(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    timeline_options = self._options
    timeline_options.metric = 'timeline'
    results = self.RunMeasurement(measurement, ps, options = timeline_options)
    self.assertEquals(0, len(results.failures))

    for category in timeline.TimelineThreadCategories.values():
      clock_time_name = timeline.ThreadTimePercentageName(category)
      clock_time = results.FindAllPageSpecificValuesNamed(clock_time_name)
      self.assertEquals(len(clock_time), 1)

      cpu_time_name = timeline.ThreadCPUTimePercentageName(category)
      cpu_time = results.FindAllPageSpecificValuesNamed(cpu_time_name)
      self.assertEquals(len(cpu_time), 1)

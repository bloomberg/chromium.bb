# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import thread_times
from measurements import smoothness_unittest
from metrics import timeline
from telemetry import decorators
from telemetry.core import wpr_modes
from telemetry.web_perf.metrics.layout import LayoutMetric
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case


class ThreadTimesUnitTest(page_test_test_case.PageTestTestCase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  @decorators.Disabled('android')
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

    for short_name in LayoutMetric.EVENTS.itervalues():
      self.assertEquals(len(results.FindAllPageSpecificValuesNamed(
        short_name + '_avg')), 1)
      self.assertEquals(len(results.FindAllPageSpecificValuesNamed(
        short_name + '_stddev')), 1)

  def testBasicForPageWithNoGesture(self):
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(smoothness_unittest.AnimatedPage(ps))

    measurement = thread_times.ThreadTimes()
    timeline_options = self._options
    results = self.RunMeasurement(measurement, ps, options = timeline_options)
    self.assertEquals(0, len(results.failures))

    for category in timeline.TimelineThreadCategories.values():
      cpu_time_name = timeline.ThreadCpuTimeResultName(category)
      cpu_time = results.FindAllPageSpecificValuesNamed(cpu_time_name)
      self.assertEquals(len(cpu_time), 1)

  def testWithSilkDetails(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = thread_times.ThreadTimes(report_silk_details=True)
    results = self.RunMeasurement(measurement, ps, options = self._options)
    self.assertEquals(0, len(results.failures))

    main_thread = "renderer_main"
    expected_trace_categories = ["blink", "cc", "idle"]
    for trace_category in expected_trace_categories:
      value_name = timeline.ThreadDetailResultName(main_thread, trace_category)
      values = results.FindAllPageSpecificValuesNamed(value_name)
      self.assertEquals(len(values), 1)

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(thread_times.ThreadTimes, self._options)

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import thread_times
from measurements import smoothness_unittest
from metrics import timeline
from telemetry.core import wpr_modes
from telemetry.unittest import options_for_unittests
from telemetry.unittest import page_test_test_case
from telemetry.unittest import test



class ThreadTimesUnitTest(page_test_test_case.PageTestTestCase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  @test.Disabled('android')
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

  def testBasicForPageWithNoGesture(self):
    ps = self.CreateEmptyPageSet()
    ps.AddPage(smoothness_unittest.AnimatedPage(ps))

    measurement = thread_times.ThreadTimes()
    timeline_options = self._options
    results = self.RunMeasurement(measurement, ps, options = timeline_options)
    self.assertEquals(0, len(results.failures))

    for category in timeline.TimelineThreadCategories.values():
      cpu_time_name = timeline.ThreadCpuTimeResultName(category)
      cpu_time = results.FindAllPageSpecificValuesNamed(cpu_time_name)
      self.assertEquals(len(cpu_time), 1)


  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(thread_times.ThreadTimes, self._options)

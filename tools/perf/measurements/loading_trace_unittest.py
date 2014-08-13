# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import loading_trace
from telemetry.core import wpr_modes
from telemetry.unittest import options_for_unittests
from telemetry.unittest import page_test_test_case

class LoadingTraceUnitTest(page_test_test_case.PageTestTestCase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testLoadingTraceBasic(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = loading_trace.LoadingTrace()
    trace_options = self._options
    results = self.RunMeasurement(measurement, ps, options = trace_options)
    self.assertEquals(0, len(results.failures))

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(loading_trace.LoadingTrace, self._options)

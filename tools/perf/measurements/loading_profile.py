# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile

from metrics import loading
from telemetry.core.platform.profiler import perf_profiler
from telemetry.page import page_test
from telemetry.value import scalar


class LoadingProfile(page_test.PageTest):
  options = {'page_repeat': 2}

  def __init__(self):
    super(LoadingProfile, self).__init__(discard_first_result=True)

  def CustomizeBrowserOptions(self, options):
    if not perf_profiler.PerfProfiler.is_supported(browser_type='any'):
      raise Exception('This measurement is not supported on this platform')

    perf_profiler.PerfProfiler.CustomizeBrowserOptions(
        browser_type='any', options=options)

  def WillNavigateToPage(self, page, tab):
    tab.browser.StartProfiling(perf_profiler.PerfProfiler.name(),
                               os.path.join(tempfile.mkdtemp(),
                                            page.file_safe_name))

  def ValidateAndMeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state,
    # but we need to wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)

    profile_files = tab.browser.StopProfiling()

    loading.LoadingMetric().AddResults(tab, results)

    profile_file = None
    for profile_file in profile_files:
      if 'renderer' in profile_file:
        break

    for function, period in perf_profiler.PerfProfiler.GetTopSamples(
        profile_file, 10).iteritems():
      results.AddValue(scalar.ScalarValue(
          results.current_page, function.replace('.', '_'), 'period', period))

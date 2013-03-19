# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_benchmark

# TODO(jeremy): Discard results from first iteration of warm tests.

# Test how long Chrome takes to load when warm.
class PerfWarm(page_benchmark.PageBenchmark):
  def __init__(self):
    super(PerfWarm, self).__init__(needs_browser_restart_after_each_run=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--reduce-security-for-dom-automation-tests')

  def MeasurePage(self, page, tab, results):
    result = tab.EvaluateJavaScript("""
      domAutomationController.getBrowserHistogram(
          "Startup.BrowserMessageLoopStartTimeFromMainEntry")
      """)
    results.Add('startup_time', 'ms', result)

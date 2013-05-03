# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from telemetry.page import page_measurement

# Test how long Chrome takes to load when warm.
class PerfWarm(page_measurement.PageMeasurement):
  def __init__(self):
    super(PerfWarm, self).__init__(needs_browser_restart_after_each_run=True,
                                   discard_first_result=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--reduce-security-for-dom-automation-tests')

  def MeasurePage(self, page, tab, results):
    result = tab.EvaluateJavaScript("""
      domAutomationController.getBrowserHistogram(
          "Startup.BrowserMessageLoopStartTimeFromMainEntry_Exact")
      """)
    result = json.loads(result)
    startup_time_ms = 0
    if 'params' in result:
      startup_time_ms = result['params']['max']
    else:
      # Support old reference builds that don't contain the new
      # Startup.BrowserMessageLoopStartTimeFromMainEntry_Exact histogram.
      result = tab.EvaluateJavaScript("""
        domAutomationController.getBrowserHistogram(
            "Startup.BrowserMessageLoopStartTimeFromMainEntry")
        """)
      result = json.loads(result)
      startup_time_ms = \
          (result['buckets'][0]['high'] + result['buckets'][0]['low']) / 2

    results.Add('startup_time', 'ms', startup_time_ms)

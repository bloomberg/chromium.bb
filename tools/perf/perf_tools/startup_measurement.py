# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from telemetry.page import page_measurement

# Test how long Chrome takes to load when warm.
class PerfWarm(page_measurement.PageMeasurement):
  HISTOGRAMS_TO_RECORD = {
    'messageloop_start_time' :
        'Startup.BrowserMessageLoopStartTimeFromMainEntry',
    'window_display_time' : 'Startup.BrowserWindowDisplay',
    'open_tabs_time' : 'Startup.BrowserOpenTabs'}

  def __init__(self):
    super(PerfWarm, self).__init__(needs_browser_restart_after_each_run=True,
                                   discard_first_result=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')

    # Old commandline flags used for reference builds.
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg(
          '--reduce-security-for-dom-automation-tests')

  def MeasurePage(self, page, tab, results):
    # TODO(jeremy): Remove references to
    # domAutomationController.getBrowserHistogram when we update the reference
    # builds.
    get_histogram_js = ('(window.statsCollectionController ?'
        'statsCollectionController :'
        'domAutomationController).getBrowserHistogram("%s")')


    for display_name, histogram_name in self.HISTOGRAMS_TO_RECORD.iteritems():
      result = tab.EvaluateJavaScript(get_histogram_js % histogram_name)
      result = json.loads(result)
      measured_time = 0

      if 'sum' in result:
        # For all the histograms logged here, there's a single entry so sum
        # is the exact value for that entry.
        measured_time = result['sum']
      elif 'buckets' in result:
        measured_time = \
            (result['buckets'][0]['high'] + result['buckets'][0]['low']) / 2

      results.Add(display_name, 'ms', measured_time)

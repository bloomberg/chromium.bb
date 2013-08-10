# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from metrics import histogram
from telemetry.page import page_measurement

_HISTOGRAMS = {
    'messageloop_start_time' :
          'Startup.BrowserMessageLoopStartTimeFromMainEntry',
    'window_display_time' : 'Startup.BrowserWindowDisplay',
    'open_tabs_time' : 'Startup.BrowserOpenTabs'}

class StartupWarm(page_measurement.PageMeasurement):
  """Test how long Chrome takes to load when warm."""

  def __init__(self):
    super(StartupWarm, self).__init__(needs_browser_restart_after_each_run=True,
                                      discard_first_result=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')

    # Old commandline flags used for reference builds.
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg(
          '--reduce-security-for-dom-automation-tests')

  def MeasurePage(self, page, tab, results):
    for display_name, histogram_name in _HISTOGRAMS.iteritems():
      histogram_data = json.loads(histogram.GetHistogramData(
          histogram.BROWSER_HISTOGRAM, histogram_name, tab))
      measured_time = 0

      if 'sum' in histogram:
        # For all the histograms logged here, there's a single entry so sum
        # is the exact value for that entry.
        measured_time = histogram_data['sum']
      elif 'buckets' in histogram_data:
        measured_time = ((histogram_data['buckets'][0]['high'] +
                         histogram_data['buckets'][0]['low']) / 2)

      results.Add(display_name, 'ms', measured_time)

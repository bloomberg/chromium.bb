# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

from metrics import Metric


class StartupMetric(Metric):
  "A metric for browser startup time."

  HISTOGRAMS_TO_RECORD = {
    'messageloop_start_time' :
        'Startup.BrowserMessageLoopStartTimeFromMainEntry',
    'window_display_time' : 'Startup.BrowserWindowDisplay',
    'open_tabs_time' : 'Startup.BrowserOpenTabs'}

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def AddResults(self, tab, results):
    get_histogram_js = 'statsCollectionController.getBrowserHistogram("%s")'

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

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import json
import logging

from metrics import Metric
from metrics import histogram_util

from telemetry.core import util


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

  def _GetBrowserMainEntryTime(self, tab):
    """Returns the main entry time (in ms) of the browser."""
    histogram_type = histogram_util.BROWSER_HISTOGRAM
    high_bytes = histogram_util.GetHistogramSum(
        histogram_type,
        'Startup.BrowserMainEntryTimeAbsoluteHighWord',
        tab)
    low_bytes = histogram_util.GetHistogramSum(
        histogram_type,
        'Startup.BrowserMainEntryTimeAbsoluteLowWord',
        tab)
    if high_bytes == 0 and low_bytes == 0:
      return None
    return (int(high_bytes) << 32) | (int(low_bytes) << 1)

  # pylint: disable=W0101
  def _RecordTabLoadTimes(self, tab, browser_main_entry_time_ms, results):
    """Records the tab load times for the browser. """
    tab_load_times = []
    TabLoadTime = collections.namedtuple(
        'TabLoadTime',
        ['load_start_ms', 'load_duration_ms', 'is_foreground_tab'])
    num_open_tabs = len(tab.browser.tabs)
    for i in xrange(num_open_tabs):
      try:
        t = tab.browser.tabs[i]
        t.WaitForDocumentReadyStateToBeComplete()

        result = t.EvaluateJavaScript(
            'statsCollectionController.tabLoadTiming()')
        result = json.loads(result)

        if 'load_start_ms' not in result or 'load_duration_ms' not in result:
          raise Exception("Outdated Chrome version, "
              "statsCollectionController.tabLoadTiming() not present")
          return
        if result['load_duration_ms'] is None:
          tab_title = t.EvaluateJavaScript('document.title')
          print "Page: ", tab_title, " didn't finish loading."
          continue

        is_foreground_tab = t.EvaluateJavaScript('!document.hidden')
        tab_load_times.append(TabLoadTime(
            int(result['load_start_ms']),
            int(result['load_duration_ms']),
            is_foreground_tab))
      except util.TimeoutException:
        # Low memory Android devices may not be able to load more than
        # one tab at a time, so may timeout when the test attempts to
        # access a background tab. Ignore these tabs.
        logging.error("Tab number: %d timed out on JavaScript access" % i)
        continue

    # Postprocess results
    load_complete_times = (
        [t.load_start_ms + t.load_duration_ms for t in tab_load_times])
    load_complete_times.sort()

    if 'android' in tab.browser.browser_type:
      # document.hidden is broken on Android - crbug.com/322544.
      foreground_tab_stats = [tab_load_times[0]]
    else:
      foreground_tab_stats = (
          [t for t in tab_load_times if t.is_foreground_tab == True])
    if (len(foreground_tab_stats) != 1):
      raise Exception ("More than one foreground tab? ", foreground_tab_stats)
    foreground_tab_stats = foreground_tab_stats[0]

    # Report load stats.
    foreground_tab_load_complete = ((foreground_tab_stats.load_start_ms +
        foreground_tab_stats.load_duration_ms) - browser_main_entry_time_ms)

    results.Add(
        'foreground_tab_load_complete', 'ms', foreground_tab_load_complete)

    if num_open_tabs > 1:
      last_tab_load_complete = (
          load_complete_times[-1] - browser_main_entry_time_ms)
      results.Add('last_tab_load_complete', 'ms', last_tab_load_complete)

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

    # Get tab load times.
    browser_main_entry_time_ms = self._GetBrowserMainEntryTime(tab)
    if (browser_main_entry_time_ms is None):
      print "Outdated Chrome version, browser main entry time not supported."
      return
    self._RecordTabLoadTimes(tab, browser_main_entry_time_ms, results)

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The tab switching measurement.

This measurement opens pages in different tabs. After all the tabs have opened,
it cycles through each tab in sequence, and records a histogram of the time
between when a tab was first requested to be shown, and when it was painted.
"""

import time

from metrics import cpu
from metrics import histogram_util
from telemetry.core import util
from telemetry.page import page_measurement

# TODO: Revisit this test once multitab support is finalized.

class TabSwitching(page_measurement.PageMeasurement):
  def __init__(self):
    super(TabSwitching, self).__init__()
    self._cpu_metric = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-stats-collection-bindings'
    ])

  def TabForPage(self, page, browser):
    return browser.tabs.New()

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.CpuMetric(browser)

  def MeasurePage(self, page, tab, results):
    """On the last tab, cycle through each tab that was opened and then record
    a single histogram for the tab switching metric."""
    if len(tab.browser.tabs) != len(page.page_set.pages):
      return
    self._cpu_metric.Start(page, tab)
    time.sleep(.5)
    self._cpu_metric.Stop(page, tab)
    # Calculate the idle cpu load before any actions are done.
    self._cpu_metric.AddResults(tab, results,
                                'idle_cpu_utilization')

    histogram_name = 'MPArch.RWH_TabSwitchPaintDuration'
    histogram_type = histogram_util.BROWSER_HISTOGRAM
    display_name = 'MPArch_RWH_TabSwitchPaintDuration'
    first_histogram = histogram_util.GetHistogram(
        histogram_type, histogram_name, tab)
    prev_histogram = first_histogram

    for i in xrange(len(tab.browser.tabs)):
      t = tab.browser.tabs[i]
      t.Activate()
      def _IsDone():
        cur_histogram = histogram_util.GetHistogram(
            histogram_type, histogram_name, tab)
        diff_histogram = histogram_util.SubtractHistogram(
            cur_histogram, prev_histogram)
        return diff_histogram
      util.WaitFor(_IsDone, 30)
      prev_histogram = histogram_util.GetHistogram(
          histogram_type, histogram_name, tab)

    last_histogram = histogram_util.GetHistogram(
        histogram_type, histogram_name, tab)
    diff_histogram = histogram_util.SubtractHistogram(last_histogram,
        first_histogram)

    results.AddSummary(display_name, '', diff_histogram,
        data_type='unimportant-histogram')

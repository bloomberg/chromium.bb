# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Multi tab memory test.

This test is a multi tab test, but we're interested in measurements for
the entire test rather than each single page.
"""

import os

from metrics import memory
from telemetry.page import page_measurement
from telemetry.page import page_set

class MemoryMultiTab(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(MemoryMultiTab, self).__init__(*args, **kwargs)
    self._memory_metric = None
    # _first_tab is used to make memory measurements
    self._first_tab = None

  def CreatePageSet(self, _, options):
    return page_set.PageSet.FromDict({
      "description": "Key mobile sites",
      "archive_data_file": "../data/key_mobile_sites.json",
      "credentials_path": "../data/credentials.json",
      "user_agent_type": "mobile",
      "pages": [
        { "url": "https://www.google.com/#hl=en&q=barack+obama" },
        { "url": "http://theverge.com" },
        { "url": "http://techcrunch.com" }
      ]}, os.path.abspath(__file__))

  def DidStartBrowser(self, browser):
    self._memory_metric = memory.MemoryMetric(browser)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    # Since this is a memory benchmark, we want to sample memory histograms at
    # a high frequency.
    options.AppendExtraBrowserArgs('--memory-metrics')

  def TabForPage(self, page, browser):
    return browser.tabs.New()

  def DidNavigateToPage(self, page, tab):
    # Start measurement on the first tab.
    if not self._first_tab:
      self._memory_metric.Start(page, tab)
      self._first_tab = tab

  def MeasurePage(self, page, tab, results):
    # Finalize measurement on the last tab.
    if len(tab.browser.tabs) == len(page.page_set.pages):
      self._memory_metric.Stop(page, self._first_tab)
      self._memory_metric.AddResults(self._first_tab, results)

  def DidRunTest(self, browser, results):
    self._memory_metric.AddSummaryResults(results)

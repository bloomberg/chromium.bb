# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Multi tab memory test.

This test is a multi tab test. When http://crbug.com/249906 is fixed,
this should be re-written to use that system. In the interim, this
tricks the page runner into running for only the first page,
then hand-navigate to other pages to do the multi tab portion of
the test. The results for the entire test are output as if
it is for the first page.
"""

import time
import os

from metrics import memory
from telemetry.page import page_measurement
from telemetry.page import page_runner
from telemetry.page import page_set

class MemoryMultiTab(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(MemoryMultiTab, self).__init__(*args, **kwargs)
    self._memory_metric = None

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

  def CanRunForPage(self, page):
    return page is page.page_set[0]

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    # Skip the first page already loaded by the runner.
    for cur_page in page.page_set[1:]:
      # TODO(bulach): remove the sleep once there's a mechanism to ensure
      # the frame has been rendered.
      time.sleep(10)

      t = tab.browser.tabs.New()

      # We create a test_stub to be able to call 'navigate_steps' on pages
      test_stub = page_measurement.PageMeasurement()
      page_state = page_runner.PageState()
      page_state.PreparePage(cur_page, t)
      page_state.ImplicitPageNavigation(cur_page, t, test_stub)

  def MeasurePage(self, page, tab, results):
    # Measure page before tabs are killed.
    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)

  def DidRunTest(self, tab, results):
    self._memory_metric.AddSummaryResults(results)

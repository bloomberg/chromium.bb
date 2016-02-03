# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Multi tab memory test.

This test is a multi tab test, but we're interested in measurements for
the entire test rather than each single page.
"""

from telemetry.page import page_test

from metrics import memory


class MemoryMultiTab(page_test.PageTest):

  def __init__(self):
    super(MemoryMultiTab, self).__init__()
    self._memory_metric = None
    # _first_tab is used to make memory measurements
    self._first_tab = None

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

  def ValidateAndMeasurePage(self, page, tab, results):
    # Finalize measurement on the last tab.
    if len(tab.browser.tabs) == len(page.story_set.stories):
      self._memory_metric.Stop(page, self._first_tab)
      self._memory_metric.AddResults(self._first_tab, results)

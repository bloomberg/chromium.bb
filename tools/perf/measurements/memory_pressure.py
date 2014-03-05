# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Multi tab memory test.

This test is a multi tab test, but we're interested in measurements for
the entire test rather than each single page.
"""

import logging

from metrics import histogram_util
from telemetry.page import page_measurement


class MemoryPressure(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(MemoryPressure, self).__init__(*args, **kwargs)
    # _first_tab is used to access histograms
    self._is_first_page = True
    self._tab_count = 0

  # Allow histogram collection
  def CustomizeBrowserOptions(self, options):
    histogram_util.CustomizeBrowserOptions(options)

  # Open a new tab at each page
  def TabForPage(self, page, browser):
    return browser.tabs.New()

  def GetTabsHistogramCounts(self, tab):
    histogram_type = histogram_util.BROWSER_HISTOGRAM
    discard_count = histogram_util.GetHistogramCount(
      histogram_type, "Tabs.Discard.DiscardCount", tab)
    kill_count = histogram_util.GetHistogramCount(
      histogram_type, "Tabs.SadTab.KillCreated", tab)
    return (discard_count, kill_count)

  def MeasurePage(self, page, tab, results):
    # After navigating to each page, check if it triggered tab discards or
    # kills.
    (discard_count, kill_count) = self.GetTabsHistogramCounts(tab)

    # Sanity check for first page
    if self._is_first_page:
      self._is_first_page = False
      assert discard_count == 0 and kill_count == 0

    self._tab_count += 1
    # End the test at the first kill or discard.
    if kill_count > 0 or discard_count > 0:
      logging.info("test ending at tab %d, discards = %d, kills = %d" %
        (self._tab_count, discard_count, kill_count))
      self.RequestExit()

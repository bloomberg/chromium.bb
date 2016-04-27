# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.page import legacy_page_test

from metrics import memory
from metrics import power


class Memory(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(Memory, self).__init__()
    self._memory_metric = None
    self._power_metric = None

  def WillStartBrowser(self, platform):
    self._power_metric = power.PowerMetric(platform)

  def DidStartBrowser(self, browser):
    self._memory_metric = memory.MemoryMetric(browser)

  def WillNavigateToPage(self, page, tab):
    tab.CollectGarbage()

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    self._power_metric.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    # Since this is a memory benchmark, we want to sample memory histograms at
    # a high frequency.
    options.AppendExtraBrowserArgs('--memory-metrics')

  def ValidateAndMeasurePage(self, page, tab, results):
    self._power_metric.Stop(page, tab)
    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)
    self._power_metric.AddResults(tab, results)

  def DidRunPage(self, platform):
    # TODO(rnephew): Remove when crbug.com/553601 is solved.
    logging.info('DidRunPage for memory metric')
    self._power_metric.Close()

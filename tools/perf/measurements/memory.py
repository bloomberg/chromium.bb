# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import memory
from metrics import power
from telemetry.page import page_test

class Memory(page_test.PageTest):
  def __init__(self):
    super(Memory, self).__init__('RunStressMemory')
    self._memory_metric = None
    self._power_metric = None

  def WillStartBrowser(self, platform):
    self._power_metric = power.PowerMetric(platform)

  def DidStartBrowser(self, browser):
    self._memory_metric = memory.MemoryMetric(browser)

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

# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import memory
from metrics import power
from telemetry.page import page_measurement

class Memory(page_measurement.PageMeasurement):
  def __init__(self):
    super(Memory, self).__init__('RunStressMemory')
    self._memory_metric = None
    self._power_metric = None

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

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

  def MeasurePage(self, page, tab, results):
    self._power_metric.Stop(page, tab)
    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)
    self._power_metric.AddResults(tab, results)

    if tab.browser.is_profiler_active('tcmalloc-heap'):
      # The tcmalloc_heap_profiler dumps files at regular
      # intervals (~20 secs).
      # This is a minor optimization to ensure it'll dump the last file when
      # the test completes.
      tab.ExecuteJavaScript("""
        if (chrome && chrome.memoryBenchmarking) {
          chrome.memoryBenchmarking.heapProfilerDump('renderer', 'final');
          chrome.memoryBenchmarking.heapProfilerDump('browser', 'final');
        }
      """)

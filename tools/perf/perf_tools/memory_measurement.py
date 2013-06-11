# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import histogram_metric
from telemetry.page import page_measurement

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'},
    {'name': 'Memory.RendererUsed', 'units': 'kb'}]

BROWSER_MEMORY_HISTOGRAMS =  [
    {'name': 'Memory.BrowserUsed', 'units': 'kb'}]

class MemoryMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(MemoryMeasurement, self).__init__('stress_memory')
    self.histograms = (
        [histogram_metric.HistogramMetric(
            h, histogram_metric.RENDERER_HISTOGRAM)
         for h in MEMORY_HISTOGRAMS] +
        [histogram_metric.HistogramMetric(
            h, histogram_metric.BROWSER_HISTOGRAM)
         for h in BROWSER_MEMORY_HISTOGRAMS])

  def DidNavigateToPage(self, page, tab):
    for h in self.histograms:
      h.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')
    options.AppendExtraBrowserArg('--enable-memory-benchmarking')
    # For a hard-coded set of Google pages (such as GMail), we produce custom
    # memory histograms (V8.Something_gmail) instead of the generic histograms
    # (V8.Something), if we detect that a renderer is only rendering this page
    # and no other pages. For this test, we need to disable histogram
    # customizing, so that we get the same generic histograms produced for all
    # pages.
    options.AppendExtraBrowserArg('--disable-histogram-customizer')
    options.AppendExtraBrowserArg('--memory-metrics')

    # Old commandline flags used for reference builds.
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg(
          '--reduce-security-for-dom-automation-tests')

  def CanRunForPage(self, page):
    return hasattr(page, 'stress_memory')

  def MeasurePage(self, page, tab, results):
    for h in self.histograms:
      h.GetValue(page, tab, results)

    if tab.browser.is_profiler_active('tcmalloc-heap'):
      # The tcmalloc_heap_profiler dumps files at regular
      # intervals (~20 secs).
      # This is a minor optimization to ensure it'll dump the last file when
      # the test completes.
      tab.ExecuteJavaScript("""
        if (chrome && chrome.memoryBenchmarking) {
          chrome.memoryBenchmarking.heapProfilerDump('final', 'renderer');
          chrome.memoryBenchmarking.heapProfilerDump('final', 'browser');
        }
      """)

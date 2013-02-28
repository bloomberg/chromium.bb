# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import histogram_measurement
from telemetry.page import page_benchmark

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'},
    {'name': 'Memory.RendererUsed', 'units': 'kb'}]

BROWSER_MEMORY_HISTOGRAMS =  [
    {'name': 'Memory.BrowserUsed', 'units': 'kb'}]

class MemoryBenchmark(page_benchmark.PageBenchmark):
  def __init__(self):
    super(MemoryBenchmark, self).__init__('stress_memory')
    self.histograms = (
        [histogram_measurement.HistogramMeasurement(
            h, histogram_measurement.RENDERER_HISTOGRAM)
         for h in MEMORY_HISTOGRAMS] +
        [histogram_measurement.HistogramMeasurement(
            h, histogram_measurement.BROWSER_HISTOGRAM)
         for h in BROWSER_MEMORY_HISTOGRAMS])

  def DidNavigateToPage(self, page, tab):
    for h in self.histograms:
      h.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    # For a hard-coded set of Google pages (such as GMail), we produce custom
    # memory histograms (V8.Something_gmail) instead of the generic histograms
    # (V8.Something), if we detect that a renderer is only rendering this page
    # and no other pages. For this test, we need to disable histogram
    # customizing, so that we get the same generic histograms produced for all
    # pages.
    options.AppendExtraBrowserArg('--disable-histogram-customizer')
    options.AppendExtraBrowserArg('--memory-metrics')
    options.AppendExtraBrowserArg('--reduce-security-for-dom-automation-tests')

  def CanRunForPage(self, page):
    return hasattr(page, 'stress_memory')

  def MeasurePage(self, page, tab, results):
    for h in self.histograms:
      h.GetValue(page, tab, results)

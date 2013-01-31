# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import multi_page_benchmark

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'},
    {'name': 'Memory.RendererUsed', 'units': 'kb'}]

BROWSER_MEMORY_HISTOGRAMS =  [
    {'name': 'Memory.BrowserUsed', 'units': 'kb'}]

class MemoryBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def __init__(self):
    super(MemoryBenchmark, self).__init__('stress_memory')

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

  def CanRunForPage(self, page):
    return hasattr(page, 'stress_memory')

  def MeasurePage(self, page, tab, results):
    for histogram in MEMORY_HISTOGRAMS:
      self._GetHistogramFromDomAutomation(tab, 'getHistogram', histogram,
                                          results)
    for histogram in BROWSER_MEMORY_HISTOGRAMS:
      self._GetHistogramFromDomAutomation(tab, 'getBrowserHistogram', histogram,
                                          results)

  def _GetHistogramFromDomAutomation(self, tab, func, histogram, results):
    name = histogram['name']
    js = ('window.domAutomationController.%s ? '
          'window.domAutomationController.%s("%s") : ""' % (func, func, name))
    data = tab.EvaluateJavaScript(js)
    if data:
      results.Add(name.replace('.', '_'), histogram['units'], data,
                  data_type='histogram')

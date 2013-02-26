# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import util
from telemetry.page import page_benchmark

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'}]

class PageCycler(page_benchmark.PageBenchmark):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--js-flags=--expose_gc')

  def MeasurePage(self, _, tab, results):
    def _IsDone():
      return tab.GetCookieByName('__pc_done') == '1'
    util.WaitFor(_IsDone, 1200, poll_interval=5)
    print 'Pages: [%s]' % tab.GetCookieByName('__pc_pages')

    # TODO(tonyg): Get IO and memory statistics.

    for histogram in MEMORY_HISTOGRAMS:
      name = histogram['name']
      data = tab.EvaluateJavaScript(
          'window.domAutomationController.getHistogram("%s")' % name)
      results.Add(name, histogram['units'], data, data_type='histogram')

    def _IsNavigatedToReport():
      return tab.GetCookieByName('__navigated_to_report') == '1'
    util.WaitFor(_IsNavigatedToReport, 60, poll_interval=5)
    timings = tab.EvaluateJavaScript('__get_timings()').split(',')
    results.Add('t', 'ms', [int(t) for t in timings], chart_name='times')

# TODO(tonyg): Add version that runs with extension profile.

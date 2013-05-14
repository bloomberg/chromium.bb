# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler measurement.

This measurement registers a window load handler in which is forces a layout and
then records the value of performance.now(). This call to now() measures the
time from navigationStart (immediately after the previous page's beforeunload
event) until after the layout in the page's load event. In addition, two garbage
collections are performed in between the page loads (in the beforeunload event).
This extra garbage collection time is not included in the measurement times.

Finally, various memory and IO statistics are gathered at the very end of
cycling all pages.
"""

import os
import sys

from perf_tools import histogram_metric
from telemetry.core import util
from telemetry.page import page_measurement

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'}]

class PageCycler(page_measurement.PageMeasurement):
  def AddCommandLineOptions(self, parser):
    # The page cyclers should default to 10 iterations. In order to change the
    # default of an option, we must remove and re-add it.
    pageset_repeat_option = parser.get_option('--pageset-repeat')
    pageset_repeat_option.default = 10
    parser.remove_option('--pageset-repeat')
    parser.add_option(pageset_repeat_option)

  def WillRunPageSet(self, tab, results):
    # Avoid paying for a cross-renderer navigation on the first page on legacy
    # page cyclers which use the filesystem.
    if tab.browser.http_server:
      tab.Navigate(tab.browser.http_server.UrlOf('nonexistent.html'))

    with open(os.path.join(os.path.dirname(__file__),
                           'page_cycler.js'), 'r') as f:
      self.page_cycler_js = f.read()  # pylint: disable=W0201

    # pylint: disable=W0201
    self.start_commit_charge = tab.browser.memory_stats['SystemCommitCharge']

    # pylint: disable=W0201
    self.histograms = [histogram_metric.HistogramMetric(
                           h, histogram_metric.RENDERER_HISTOGRAM)
                       for h in MEMORY_HISTOGRAMS]

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self.page_cycler_js

  def DidNavigateToPage(self, page, tab):
    for h in self.histograms:
      h.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--js-flags=--expose_gc')
    options.AppendExtraBrowserArg('--no-sandbox')
    # Temporarily enable threaded compositing on Mac on only some page sets.
    # This malignancy is to diagnose an issue where the bots are experiencing
    # a regression that isn't reproducing locally.
    # TODO(ccameron): delete this
    # http://crbug.com/180025
    if sys.platform == 'darwin':
      composited_page_sets = ('/bloat.json', '/moz.json', '/intl2.json')
      if sys.argv[-1].endswith(composited_page_sets):
        options.AppendExtraBrowserArg('--force-compositing-mode')
        options.AppendExtraBrowserArg('--enable-threaded-compositing')
      else:
        options.AppendExtraBrowserArg('--disable-force-compositing-mode')

  def MeasureMemory(self, tab, results):
    memory = tab.browser.memory_stats
    if not memory['Browser']:
      return

    metric = 'resident_set_size'
    if sys.platform == 'win32':
      metric = 'working_set'

    def AddSummariesForProcessTypes(process_types_memory, process_type_trace):
      def AddSummary(value_name_memory, value_name_trace):
        if len(process_types_memory) > 1 and value_name_memory.endswith('Peak'):
          return
        values = []
        for process_type_memory in process_types_memory:
          if value_name_memory in memory[process_type_memory]:
            values.append(memory[process_type_memory][value_name_memory])
        if values:
          results.AddSummary(value_name_trace + process_type_trace,
                             'bytes', sum(values), data_type='unimportant')
      AddSummary('VM', 'vm_final_size_')
      AddSummary('WorkingSetSize', 'vm_%s_final_size_' % metric)
      AddSummary('PrivateDirty', 'vm_private_dirty_final_')
      AddSummary('ProportionalSetSize', 'vm_proportional_set_size_final_')
      AddSummary('VMPeak', 'vm_peak_size_')
      AddSummary('WorkingSetSizePeak', '%s_peak_size_' % metric)

    AddSummariesForProcessTypes(['Browser'], 'browser')
    AddSummariesForProcessTypes(['Renderer'], 'renderer')
    AddSummariesForProcessTypes(['Gpu'], 'gpu')
    AddSummariesForProcessTypes(['Browser', 'Renderer', 'Gpu'], 'total')

    results.AddSummary('commit_charge', 'kb',
                       memory['SystemCommitCharge'] - self.start_commit_charge,
                       data_type='unimportant')
    results.AddSummary('processes', 'count', memory['ProcessCount'],
                       data_type='unimportant')

  def MeasureIO(self, tab, results):
    io_stats = tab.browser.io_stats
    if not io_stats['Browser']:
      return

    def AddSummariesForProcessType(process_type_io, process_type_trace):
      results.AddSummary('read_operations_' + process_type_trace, '',
                         io_stats[process_type_io]
                         ['ReadOperationCount'],
                         data_type='unimportant')
      results.AddSummary('write_operations_' + process_type_trace, '',
                         io_stats[process_type_io]
                         ['WriteOperationCount'],
                         data_type='unimportant')
      results.AddSummary('read_bytes_' + process_type_trace, 'kb',
                         io_stats[process_type_io]['ReadTransferCount'] / 1024,
                         data_type='unimportant')
      results.AddSummary('write_bytes_' + process_type_trace, 'kb',
                         io_stats[process_type_io]['WriteTransferCount'] / 1024,
                         data_type='unimportant')
    AddSummariesForProcessType('Browser', 'browser')
    AddSummariesForProcessType('Renderer', 'renderer')
    AddSummariesForProcessType('Gpu', 'gpu')

  def MeasurePage(self, page, tab, results):
    def _IsDone():
      return bool(tab.EvaluateJavaScript('__pc_load_time'))
    util.WaitFor(_IsDone, 1200)

    for h in self.histograms:
      h.GetValue(page, tab, results)

    results.Add('page_load_time', 'ms',
                int(float(tab.EvaluateJavaScript('__pc_load_time'))),
                chart_name='times')

  def DidRunPageSet(self, tab, results):
    self.MeasureMemory(tab, results)
    self.MeasureIO(tab, results)

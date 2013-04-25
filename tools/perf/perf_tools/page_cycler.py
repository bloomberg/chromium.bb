# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler benchmark.

This benchmark registers a window load handler in which is forces a layout and
then records the value of performance.now(). This call to now() measures the
time from navigationStart (immediately after the previous page's beforeunload
event) until after the layout in the page's load event. In addition, two garbage
collections are performed in between the page loads (in the beforeunload event).
This extra garbage collection time is not included in the benchmark times.

Finally, various memory and IO statistics are gathered at the very end of
cycling all pages.
"""

import os
import sys

from perf_tools import histogram_measurement
from telemetry.core import util
from telemetry.page import page_benchmark

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'}]

class PageCycler(page_benchmark.PageBenchmark):
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
    self.histograms = [histogram_measurement.HistogramMeasurement(
                           h, histogram_measurement.RENDERER_HISTOGRAM)
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
    # Temporarily enable threaded compositing on Mac on moz page set only.
    # This malignancy is to diagnose an issue where the bots are experiencing
    # a regression that isn't reproducing locally.
    # TODO(ccameron): delete this
    # http://crbug.com/180025
    if sys.platform == 'darwin':
      if sys.argv[-1].endswith('/moz.json'):
        options.AppendExtraBrowserArg('--force-compositing-mode')
        options.AppendExtraBrowserArg('--enable-threaded-compositing')
      else:
        options.AppendExtraBrowserArg('--disable-force-compositing-mode')

  def MeasureMemory(self, tab, results):
    memory = tab.browser.memory_stats
    if not memory['Browser']:
      return

    metric = 'rss'
    if sys.platform == 'win32':
      metric = 'ws'

    # Browser
    if 'VM' in memory['Browser']:
      results.AddSummary('vm_size_f_b', 'bytes', memory['Browser']['VM'],
                         chart_name='vm_size_final_b', data_type='unimportant')
    if 'VMPeak' in memory['Browser']:
      results.AddSummary('vm_pk_b', 'bytes', memory['Browser']['VMPeak'],
                         chart_name='vm_peak_b', data_type='unimportant')
    if 'WorkingSetSize' in memory['Browser']:
      results.AddSummary('vm_%s_f_b' % metric, 'bytes',
                         memory['Browser']['WorkingSetSize'],
                         chart_name='vm_%s_final_b' % metric,
                         data_type='unimportant')
    if 'WorkingSetSizePeak' in memory['Browser']:
      results.AddSummary('%s_pk_b' % metric, 'bytes',
                         memory['Browser']['WorkingSetSizePeak'],
                         chart_name='%s_peak_b' % metric,
                         data_type='unimportant')
    if 'PrivateDirty' in memory['Browser']:
      results.AddSummary('vm_private_dirty_f_b', 'bytes',
                         memory['Browser']['PrivateDirty'],
                         chart_name='vm_private_dirty_final_b',
                         data_type='unimportant')
    if 'ProportionalSetSize' in memory['Browser']:
      results.AddSummary('vm_pss_f_b', 'bytes',
                         memory['Browser']['ProportionalSetSize'],
                         chart_name='vm_pss_final_b', data_type='unimportant')

    # Renderer
    if 'VM' in memory['Renderer']:
      results.AddSummary('vm_size_f_r', 'bytes', memory['Renderer']['VM'],
                         chart_name='vm_size_final_r', data_type='unimportant')
    if 'VMPeak' in memory['Renderer']:
      results.AddSummary('vm_pk_r', 'bytes', memory['Browser']['VMPeak'],
                         chart_name='vm_peak_r', data_type='unimportant')
    if 'WorkingSetSize' in memory['Renderer']:
      results.AddSummary('vm_%s_f_r' % metric, 'bytes',
                         memory['Renderer']['WorkingSetSize'],
                         chart_name='vm_%s_final_r' % metric,
                         data_type='unimportant')
    if 'WorkingSetSizePeak' in memory['Renderer']:
      results.AddSummary('%s_pk_r' % metric, 'bytes',
                         memory['Browser']['WorkingSetSizePeak'],
                         chart_name='%s_peak_r' % metric,
                         data_type='unimportant')
    if 'PrivateDirty' in memory['Renderer']:
      results.AddSummary('vm_private_dirty_f_r', 'bytes',
                         memory['Renderer']['PrivateDirty'],
                         chart_name='vm_private_dirty_final_r',
                         data_type='unimportant')
    if 'ProportionalSetSize' in memory['Renderer']:
      results.AddSummary('vm_pss_f_r', 'bytes',
                         memory['Renderer']['ProportionalSetSize'],
                         chart_name='vm_pss_final_r', data_type='unimportant')

    # Total
    if 'VM' in memory['Browser'] and 'VM' in memory['Renderer']:
      results.AddSummary('vm_size_f_t', 'bytes',
                         memory['Browser']['VM'] + memory['Renderer']['VM'],
                         chart_name='vm_size_final_t', data_type='unimportant')
    if ('WorkingSetSize' in memory['Browser'] and
        'WorkingSetSize' in memory['Renderer']):
      results.AddSummary('vm_%s_f_t' % metric, 'bytes',
                         memory['Browser']['WorkingSetSize'] +
                         memory['Renderer']['WorkingSetSize'],
                         chart_name='vm_%s_final_t' % metric,
                         data_type='unimportant')
    if ('PrivateDirty' in memory['Browser'] and
        'PrivateDirty' in memory['Renderer']):
      results.AddSummary('vm_private_dirty_f_t', 'bytes',
                         memory['Browser']['PrivateDirty'] +
                         memory['Renderer']['PrivateDirty'],
                         chart_name='vm_private_dirty_final_t',
                         data_type='unimportant')
    if ('ProportionalSetSize' in memory['Browser'] and
        'ProportionalSetSize' in memory['Renderer']):
      results.AddSummary('vm_pss_f_t', 'bytes',
                         memory['Browser']['ProportionalSetSize'] +
                         memory['Renderer']['ProportionalSetSize'],
                         chart_name='vm_pss_final_t', data_type='unimportant')

    results.AddSummary('cc', 'kb',
                       memory['SystemCommitCharge'] - self.start_commit_charge,
                       chart_name='commit_charge', data_type='unimportant')
    results.AddSummary('proc_', 'count', memory['ProcessCount'],
                       chart_name='processes', data_type='unimportant')

  def MeasureIO(self, tab, results):
    io_stats = tab.browser.io_stats
    if not io_stats['Browser']:
      return
    results.AddSummary('r_op_b', '', io_stats['Browser']['ReadOperationCount'],
                       chart_name='read_op_b', data_type='unimportant')
    results.AddSummary('w_op_b', '', io_stats['Browser']['WriteOperationCount'],
                       chart_name='write_op_b', data_type='unimportant')
    results.AddSummary('r_b', 'kb',
                       io_stats['Browser']['ReadTransferCount'] / 1024,
                       chart_name='read_byte_b', data_type='unimportant')
    results.AddSummary('w_b', 'kb',
                       io_stats['Browser']['WriteTransferCount'] / 1024,
                       chart_name='write_byte_b', data_type='unimportant')
    results.AddSummary('r_op_r', '', io_stats['Renderer']['ReadOperationCount'],
                       chart_name='read_op_r', data_type='unimportant')
    results.AddSummary('w_op_r', '',
                       io_stats['Renderer']['WriteOperationCount'],
                       chart_name='write_op_r', data_type='unimportant')
    results.AddSummary('r_r', 'kb',
                       io_stats['Renderer']['ReadTransferCount'] / 1024,
                       chart_name='read_byte_r', data_type='unimportant')
    results.AddSummary('w_r', 'kb',
                       io_stats['Renderer']['WriteTransferCount'] / 1024,
                       chart_name='write_byte_r', data_type='unimportant')

  def MeasurePage(self, page, tab, results):
    def _IsDone():
      return bool(tab.EvaluateJavaScript('__pc_load_time'))
    util.WaitFor(_IsDone, 1200)

    for h in self.histograms:
      h.GetValue(page, tab, results)

    results.Add('t', 'ms', int(float(tab.EvaluateJavaScript('__pc_load_time'))),
                chart_name='times')

  def DidRunPageSet(self, tab, results):
    self.MeasureMemory(tab, results)
    self.MeasureIO(tab, results)

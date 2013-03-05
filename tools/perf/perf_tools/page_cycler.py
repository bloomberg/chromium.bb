# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

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
    options.AppendExtraBrowserArg('--no-sandbox')

  def MeasureMemory(self, tab, results):
    memory = tab.browser.memory_stats
    if not memory['Browser']:
      return

    metric = 'rss'
    if sys.platform == 'win32':
      metric = 'ws'

    # Browser
    if 'VM' in memory['Browser']:
      results.Add('vm_size_f_b', 'bytes', memory['Browser']['VM'],
                  chart_name='vm_size_final_b', data_type='unimportant')
    if 'VMPeak' in memory['Browser']:
      results.Add('vm_peak_b', 'bytes', memory['Browser']['VMPeak'],
                  chart_name='vm_pk_b', data_type='unimportant')
    if 'WorkingSetSize' in memory['Browser']:
      results.Add('vm_%s_f_b' % metric, 'bytes',
                  memory['Browser']['WorkingSetSize'],
                  chart_name='vm_%s_final_b' % metric, data_type='unimportant')
    if 'WorkingSetSizePeak' in memory['Browser']:
      results.Add('%s_peak_b' % metric, 'bytes',
                  memory['Browser']['WorkingSetSizePeak'],
                  chart_name='%s_pk_b' % metric, data_type='unimportant')
    if 'PrivateDirty' in memory['Browser']:
      results.Add('vm_private_dirty_f_b', 'bytes',
                  memory['Browser']['PrivateDirty'],
                  chart_name='vm_private_dirty_final_b',
                  data_type='unimportant')
    if 'ProportionalSetSize' in memory['Browser']:
      results.Add('vm_pss_f_b', 'bytes',
                  memory['Browser']['ProportionalSetSize'],
                  chart_name='vm_pss_final_b', data_type='unimportant')

    # Renderer
    if 'VM' in memory['Renderer']:
      results.Add('vm_size_f_r', 'bytes', memory['Renderer']['VM'],
                  chart_name='vm_size_final_r', data_type='unimportant')
    if 'VMPeak' in memory['Renderer']:
      results.Add('vm_peak_r', 'bytes', memory['Browser']['VMPeak'],
                  chart_name='vm_pk_r', data_type='unimportant')
    if 'WorkingSetSize' in memory['Renderer']:
      results.Add('vm_%s_f_r' % metric, 'bytes',
                  memory['Renderer']['WorkingSetSize'],
                  chart_name='vm_%s_final_r' % metric, data_type='unimportant')
    if 'WorkingSetSizePeak' in memory['Renderer']:
      results.Add('%s_peak_r' % metric, 'bytes',
                  memory['Browser']['WorkingSetSizePeak'],
                  chart_name='%s_pk_r' % metric, data_type='unimportant')
    if 'PrivateDirty' in memory['Renderer']:
      results.Add('vm_private_dirty_f_r', 'bytes',
                  memory['Renderer']['PrivateDirty'],
                  chart_name='vm_private_dirty_final_r',
                  data_type='unimportant')
    if 'ProportionalSetSize' in memory['Renderer']:
      results.Add('vm_pss_f_r', 'bytes',
                  memory['Renderer']['ProportionalSetSize'],
                  chart_name='vm_pss_final_r', data_type='unimportant')

    # Total
    if 'VM' in memory['Browser'] and 'WM' in memory['Renderer']:
      results.Add('vm_size_f_t', 'bytes',
                  memory['Browser']['VM'] + memory['Renderer']['VM'],
                  chart_name='vm_size_final_t', data_type='unimportant')
    if ('WorkingSetSize' in memory['Browser'] and
        'WorkingSetSize' in memory['Renderer']):
      results.Add('vm_%s_f_t' % metric, 'bytes',
                  memory['Browser']['WorkingSetSize'] +
                  memory['Renderer']['WorkingSetSize'],
                  chart_name='vm_%s_final_t' % metric, data_type='unimportant')
    if ('PrivateDirty' in memory['Browser'] and
        'PrivateDirty' in memory['Renderer']):
      results.Add('vm_private_dirty_f_t', 'bytes',
                  memory['Browser']['PrivateDirty'] +
                  memory['Renderer']['PrivateDirty'],
                  chart_name='vm_private_dirty_final_t',
                  data_type='unimportant')
    if ('ProportionalSetSize' in memory['Browser'] and
        'ProportionalSetSize' in memory['Renderer']):
      results.Add('vm_pss_f_t', 'bytes',
                  memory['Browser']['ProportionalSetSize'] +
                  memory['Renderer']['ProportionalSetSize'],
                  chart_name='vm_pss_final_t', data_type='unimportant')

    results.Add('cc', 'kb', memory['SystemCommitCharge'],
                chart_name='commit_charge', data_type='unimportant')
    results.Add('proc_', 'count', memory['ProcessCount'],
                chart_name='processes', data_type='unimportant')

  def MeasureIO(self, tab, results):
    io_stats = tab.browser.io_stats
    if not io_stats['Browser']:
      return
    results.Add('r_op_b', '', io_stats['Browser']['ReadOperationCount'],
                chart_name='read_op_b', data_type='unimportant')
    results.Add('w_op_b', '', io_stats['Browser']['WriteOperationCount'],
                chart_name='write_op_b', data_type='unimportant')
    results.Add('r_b', 'kb', io_stats['Browser']['ReadTransferCount'],
                chart_name='read_byte_b', data_type='unimportant')
    results.Add('w_b', 'kb', io_stats['Browser']['WriteTransferCount'],
                chart_name='write_byte_b', data_type='unimportant')
    results.Add('r_op_r', '', io_stats['Renderer']['ReadOperationCount'],
                chart_name='read_op_r', data_type='unimportant')
    results.Add('w_op_r', '', io_stats['Renderer']['WriteOperationCount'],
                chart_name='write_op_r', data_type='unimportant')
    results.Add('r_r', 'kb', io_stats['Renderer']['ReadOperationCount'],
                chart_name='read_byte_r', data_type='unimportant')
    results.Add('w_r', 'kb', io_stats['Renderer']['WriteOperationCount'],
                chart_name='write_byte_r', data_type='unimportant')

  def MeasurePage(self, _, tab, results):
    def _IsDone():
      return tab.GetCookieByName('__pc_done') == '1'
    util.WaitFor(_IsDone, 1200, poll_interval=5)
    print 'Pages: [%s]' % tab.GetCookieByName('__pc_pages')

    self.MeasureMemory(tab, results)
    self.MeasureIO(tab, results)

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

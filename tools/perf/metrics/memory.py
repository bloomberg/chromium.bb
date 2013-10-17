# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from metrics import histogram_util
from metrics import Metric

_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent',
     'type': histogram_util.RENDERER_HISTOGRAM},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb',
     'type': histogram_util.RENDERER_HISTOGRAM},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb',
     'type': histogram_util.RENDERER_HISTOGRAM},
    {'name': 'Memory.RendererUsed', 'units': 'kb',
     'type': histogram_util.RENDERER_HISTOGRAM},
    {'name': 'Memory.BrowserUsed', 'units': 'kb',
     'type': histogram_util.BROWSER_HISTOGRAM}]

class MemoryMetric(Metric):
  """MemoryMetric gathers memory statistics from the browser object.

  This includes both per-page histogram stats, most about javascript
  memory usage, and overall memory stats from the system for the whole
  test run."""

  def __init__(self, browser):
    super(MemoryMetric, self).__init__()
    self._browser = browser
    self._start_commit_charge = self._browser.memory_stats['SystemCommitCharge']
    self._memory_stats = None
    self._histogram_start = dict()
    self._histogram_delta = dict()

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs([
        '--enable-stats-collection-bindings',
        '--enable-memory-benchmarking',
        # For a hard-coded set of Google pages (such as GMail), we produce
        # custom memory histograms (V8.Something_gmail) instead of the generic
        # histograms (V8.Something), if we detect that a renderer is only
        # rendering this page and no other pages. For this test, we need to
        # disable histogram customizing, so that we get the same generic
        # histograms produced for all pages.
        '--disable-histogram-customizer'
    ])

  def Start(self, page, tab):
    """Start the per-page preparation for this metric.

    Here, this consists of recording the start value of all the histograms.
    """
    for h in _HISTOGRAMS:
      histogram_data = histogram_util.GetHistogram(
          h['type'], h['name'], tab)
      # Histogram data may not be available
      if not histogram_data:
        continue
      self._histogram_start[h['name']] = histogram_data

  def Stop(self, page, tab):
    """Prepare the results for this page.

    The results are the differences between the current histogram values
    and the values when Start() was called.
    """
    assert self._histogram_start, 'Must call Start() first'
    for h in _HISTOGRAMS:
      # Histogram data may not be available
      if h['name'] not in self._histogram_start:
        continue
      histogram_data = histogram_util.GetHistogram(
          h['type'], h['name'], tab)
      self._histogram_delta[h['name']] = histogram_util.SubtractHistogram(
          histogram_data, self._histogram_start[h['name']])

  def AddResults(self, tab, results):
    """Add results for this page to the results object."""
    assert self._histogram_delta, 'Must call Stop() first'
    for h in _HISTOGRAMS:
      # Histogram data may not be available
      if h['name'] not in self._histogram_start:
        continue
      results.Add(h['name'], h['units'], self._histogram_delta[h['name']],
                  data_type='unimportant-histogram')

  def AddSummaryResults(self, results, trace_name=None):
    """Add summary (overall) results to the results object."""
    self._memory_stats = self._browser.memory_stats
    if not self._memory_stats['Browser']:
      return

    metric = 'resident_set_size'
    if sys.platform == 'win32':
      metric = 'working_set'

    def AddSummariesForProcessTypes(process_types_memory, process_type_trace):
      """Add all summaries to the results for a given set of process types.

      Args:
        process_types_memory: A list of process types, e.g. Browser, 'Renderer'
        process_type_trace: The name of this set of process types in the output
      """
      def AddSummary(value_name_memory, value_name_trace):
        """Add a summary to the results for a given statistic.

        Args:
          value_name_memory: Name of some statistic, e.g. VM, WorkingSetSize
          value_name_trace: Name of this statistic to be used in the output
        """
        if len(process_types_memory) > 1 and value_name_memory.endswith('Peak'):
          return
        values = []
        for process_type_memory in process_types_memory:
          stats = self._memory_stats[process_type_memory]
          if value_name_memory in stats:
            values.append(stats[value_name_memory])
        if values:
          if trace_name:
            current_trace = '%s_%s' % (trace_name, process_type_trace)
            chart_name = value_name_trace
          else:
            current_trace = '%s_%s' % (value_name_trace, process_type_trace)
            chart_name = current_trace
          results.AddSummary(current_trace, 'bytes', sum(values),
                             chart_name=chart_name, data_type='unimportant')

      AddSummary('VM', 'vm_final_size')
      AddSummary('WorkingSetSize', 'vm_%s_final_size' % metric)
      AddSummary('PrivateDirty', 'vm_private_dirty_final')
      AddSummary('ProportionalSetSize', 'vm_proportional_set_size_final')
      AddSummary('SharedDirty', 'vm_shared_dirty_final')
      AddSummary('VMPeak', 'vm_peak_size')
      AddSummary('WorkingSetSizePeak', '%s_peak_size' % metric)

    AddSummariesForProcessTypes(['Browser'], 'browser')
    AddSummariesForProcessTypes(['Renderer'], 'renderer')
    AddSummariesForProcessTypes(['Gpu'], 'gpu')
    AddSummariesForProcessTypes(['Browser', 'Renderer', 'Gpu'], 'total')

    end_commit_charge = self._memory_stats['SystemCommitCharge']
    commit_charge_difference = end_commit_charge - self._start_commit_charge
    results.AddSummary(trace_name or 'commit_charge', 'kb',
                       commit_charge_difference,
                       chart_name='commit_charge',
                       data_type='unimportant')
    results.AddSummary(trace_name or 'processes', 'count',
                       self._memory_stats['ProcessCount'],
                       chart_name='processes',
                       data_type='unimportant')


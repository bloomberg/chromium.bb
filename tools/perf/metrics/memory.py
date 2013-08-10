# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from metrics import Metric
from metrics import histogram

_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent',
     'type': histogram.RENDERER_HISTOGRAM},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb',
     'type': histogram.RENDERER_HISTOGRAM},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb',
     'type': histogram.RENDERER_HISTOGRAM},
    {'name': 'Memory.RendererUsed', 'units': 'kb',
     'type': histogram.RENDERER_HISTOGRAM},
    {'name': 'Memory.BrowserUsed', 'units': 'kb',
     'type': histogram.BROWSER_HISTOGRAM}]

class MemoryMetric(Metric):
  """MemoryMetric gathers memory statistics from the browser object.

  This includes both per-page histogram stats, most about javascript
  memory usage, and overall memory stats from the system for the whole
  test run."""

  def __init__(self, browser):
    super(MemoryMetric, self).__init__()
    self._browser = browser
    self._start_commit_charge = self._browser.memory_stats['SystemCommitCharge']
    self._end_memory_stats = None
    self._histogram_start_values = dict()
    self._histogram_delta_values = dict()

  def Start(self, page, tab):
    """Start the per-page preparation for this metric.

    Here, this consists of recording the start value of all the histograms.
    """
    for h in _HISTOGRAMS:
      histogram_data = histogram.GetHistogramData(h['type'], h['name'], tab)
      if not histogram_data:
        continue
      self._histogram_start_values[h['name']] = histogram_data

  def Stop(self, page, tab):
    """Prepare the results for this page.

    The results are the differences between the current histogram values
    and the values when Start() was called.
    """
    assert self._histogram_start_values, 'Must call Start() first'
    for h in _HISTOGRAMS:
      histogram_data = histogram.GetHistogramData(h['type'], h['name'], tab)
      self._histogram_delta_values = histogram.SubtractHistogram(
          histogram_data, self._histogram_start_values[h['name']])

  def AddResults(self, tab, results):
    """Add results for this page to the results object."""
    assert self._histogram_delta_values, 'Must call Stop() first'
    for h in _HISTOGRAMS:
      histogram_data = self._histogram_delta_values[h['name']]
      results.Add(h['name'], h['units'], histogram_data,
                  data_type='unimportant-histogram')

  def AddSummaryResults(self, results):
    """Add summary (overall) results to the results object."""
    self._end_memory_stats = self._browser.memory_stats
    if not self._end_memory_stats['Browser']:
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
          stats = self._end_memory_stats[process_type_memory]
          if value_name_memory in stats:
            values.append(stats[value_name_memory])
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

    end_commit_charge = self._end_memory_stats['SystemCommitCharge']
    commit_charge_difference = end_commit_charge - self._start_commit_charge
    results.AddSummary('commit_charge', 'kb', commit_charge_difference,
                       data_type='unimportant')
    results.AddSummary('processes', 'count', self._memory_stats['ProcessCount'],
                       data_type='unimportant')


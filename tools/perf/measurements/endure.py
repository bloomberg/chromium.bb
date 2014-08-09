# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import time

from metrics import v8_object_stats
from telemetry.page import page_test
from telemetry.value import scalar

# V8 statistics counter names. These can be retrieved using
# v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable.
_V8_BYTES_COMMITTED = [
    'V8.MemoryNewSpaceBytesCommitted',
    'V8.MemoryOldPointerSpaceBytesCommitted',
    'V8.MemoryOldDataSpaceBytesCommitted',
    'V8.MemoryCodeSpaceBytesCommitted',
    'V8.MemoryMapSpaceBytesCommitted',
    'V8.MemoryCellSpaceBytesCommitted',
    'V8.MemoryPropertyCellSpaceBytesCommitted',
    'V8.MemoryLoSpaceBytesCommitted',
]
_V8_BYTES_USED = [
    'V8.MemoryNewSpaceBytesUsed',
    'V8.MemoryOldPointerSpaceBytesUsed',
    'V8.MemoryOldDataSpaceBytesUsed',
    'V8.MemoryCodeSpaceBytesUsed',
    'V8.MemoryMapSpaceBytesUsed',
    'V8.MemoryCellSpaceBytesUsed',
    'V8.MemoryPropertyCellSpaceBytesUsed',
    'V8.MemoryLoSpaceBytesUsed',
]
_V8_MEMORY_ALLOCATED = [
    'V8.OsMemoryAllocated',
]


# NOTE(chrishenry): This measurement does NOT work anymore. The
# feature it depends on has been removed from telemetry. The benchmark
# has been disabled on bot.
class Endure(page_test.PageTest):

  def __init__(self):
    super(Endure, self).__init__('RunEndure')
    # Browser object, saved so that browser.memory_stats can be accessed.
    self._browser = None

    # Dictionary of trace name to lists of y-values, for making summary values.
    self._y_values = {}

    # Number of page repetitions since the start of the test.
    self._iterations_elapsed = 0

    # Start time of the test, used to report total time.
    self._start_time = None

  @classmethod
  def AddCommandLineArgs(cls, parser):
    group = optparse.OptionGroup(parser, 'Endure options')
    group.add_option('--perf-stats-interval',
                     dest='perf_stats_interval',
                     default=1,
                     type='int',
                     help='Number of iterations per sampling of statistics.')
    parser.add_option_group(group)

  def DidStartBrowser(self, browser):
    """Initializes the measurement after the browser is started."""
    self._browser = browser
    self._start_time = time.time()

  def CustomizeBrowserOptions(self, options):
    """Adds extra command-line options to the browser."""
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

  def ValidateAndMeasurePage(self, page, tab, results):
    """Takes a sample and adds a result if enough time has passed."""
    self._iterations_elapsed += 1
    if self._iterations_elapsed % int(self.options.perf_stats_interval) == 0:
      self._SampleStats(tab, results)

  def _SampleStats(self, tab, results):
    """Records information and add it to the results."""

    def AddPoint(trace_name, units_y, value_y, chart_name=None):
      """Adds one data point to the results object."""
      if chart_name:
        trace_name = '%s.%s' % (chart_name, trace_name)
      else:
        assert '.' not in trace_name, (
            'Trace names cannot contain "." with an empty chart_name since this'
            ' is used to delimit chart_name.trace_name.')
      results.AddValue(scalar.ScalarValue(
          results.current_page, trace_name + '_X', 'iterations',
          self._iterations_elapsed, important=False))
      results.AddValue(scalar.ScalarValue(
          results.current_page, trace_name + '_Y', units_y, value_y,
          important=False))

      # Save the value so that summary stats can be calculated.
      if trace_name not in self._y_values:
        self._y_values[trace_name] = {
            'units': units_y,
            'chart_name': chart_name,
            'values': [],
        }
      self._y_values[trace_name]['values'].append(value_y)

    # DOM nodes and event listeners.
    dom_stats = tab.dom_stats
    dom_node_count = dom_stats['node_count']
    event_listener_count = dom_stats['event_listener_count']
    AddPoint('dom_nodes', 'count', dom_node_count, chart_name='object_counts')
    AddPoint('event_listeners', 'count', event_listener_count,
             chart_name='object_counts')

    # Browser and renderer virtual memory stats.
    memory_stats = self._browser.memory_stats
    def BrowserVMStats(statistic_name):
      """Get VM stats from the Browser object in KB."""
      return memory_stats[statistic_name].get('VM', 0) / 1024.0
    AddPoint('browser_vm', 'KB', BrowserVMStats('Browser'),
             chart_name='vm_stats')
    AddPoint('renderer_vm', 'KB', BrowserVMStats('Renderer'),
             chart_name='vm_stats')
    AddPoint('gpu_vm', 'KB', BrowserVMStats('Gpu'), chart_name='vm_stats')

    # V8 counter stats.
    def V8StatsSum(counters):
      """Given a list of V8 counter names, get the sum of the values in KB."""
      stats = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(tab, counters)
      return sum(stats.values()) / 1024.0
    AddPoint('v8_memory_committed', 'KB', V8StatsSum(_V8_BYTES_COMMITTED),
             chart_name='v8_counter_stats')
    AddPoint('v8_memory_used', 'KB', V8StatsSum(_V8_BYTES_USED),
             chart_name='v8_counter_stats')
    AddPoint('v8_memory_allocated', 'KB', V8StatsSum(_V8_MEMORY_ALLOCATED),
             chart_name='v8_counter_stats')

  def DidRunTest(self, browser, results):
    """Adds summary results (single number for one test run)."""
    # Report test run length.
    results.AddSummaryValue(scalar.ScalarValue(None, 'total_iterations',
                                               'iterations',
                                               self._iterations_elapsed,
                                               important=False))
    results.AddSummaryValue(scalar.ScalarValue(None, 'total_time', 'seconds',
                                               time.time() - self._start_time,
                                               important=False))

    # Add summary stats which could be monitored for anomalies.
    for trace_name in self._y_values:
      units = self._y_values[trace_name]['units']
      chart_name = self._y_values[trace_name]['chart_name']
      values = self._y_values[trace_name]['values']
      value_name = '%s.%s_max' % (chart_name, trace_name)
      results.AddSummaryValue(
          scalar.ScalarValue(None, value_name, units, max(values)))

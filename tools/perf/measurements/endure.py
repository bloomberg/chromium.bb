# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import re
import time

from metrics import v8_object_stats
from telemetry.page import page_measurement

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


class Endure(page_measurement.PageMeasurement):
  def __init__(self):
    super(Endure, self).__init__('endure')
    # Browser object, saved so that browser.memory_stats can be accessed.
    self._browser = None

    # Dict of trace name to lists of y-values, for making summary values.
    self._y_values = {}

    # Time of test start and last sample, in seconds since the epoch.
    self._start_time = None
    self._last_sample_time = 0

    # Number of page repetitions currently, and at the last sample.
    self._iterations = 0
    self._last_sample_iterations = 0

  @classmethod
  def AddCommandLineArgs(cls, parser):
    group = optparse.OptionGroup(parser, 'Endure options')
    group.add_option('--perf-stats-interval',
                     dest='perf_stats_interval',
                     default='20s',
                     type='string',
                     help='Interval between sampling of statistics, either in '
                          'seconds (specified by appending \'s\') or in number '
                          'of iterations')
    parser.add_option_group(group)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    """Parses the --perf-stats-interval option that was passed in."""
    cls._interval_seconds = None
    cls._interval_iterations = None

    match = re.match('([0-9]+)([sS]?)$', args.perf_stats_interval)
    if not match:
      parser.error('Invalid value for --perf-stats-interval: %s' %
                   args.perf_stats_interval)

    if match.group(2):
      cls._interval_seconds = int(match.group(1))
    else:
      cls._interval_iterations = int(match.group(1))
    assert cls._interval_seconds or cls._interval_iterations

  def DidStartBrowser(self, browser):
    """Saves the Browser object. Called after the browser is started."""
    self._browser = browser

  def CustomizeBrowserOptions(self, options):
    """Adds extra command-line options to the browser."""
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    """Checks whether a page has the required 'endure' property."""
    return hasattr(page, 'endure')

  def WillRunPageRepeats(self, page):
    """Set-up before starting a new page."""
    # Reset the starting time for each new page.
    self._start_time = time.time()

    # Prefix the page name so it can be picked up by the buildbot script that
    # parses Endure output.
    if page.name and not page.display_name.startswith('endure_'):
      page.name = 'endure_' + page.name

  def MeasurePage(self, page, tab, results):
    """Takes a sample and adds a result if enough time has passed."""
    # Check whether the sample interval is specified in seconds or iterations,
    # and take a sample if it's time.
    self._iterations += 1
    if self._interval_seconds:
      now = time.time()
      seconds_elapsed = int(round(now - self._last_sample_time))
      # Note: the time since last sample must be at least as many seconds
      # as specified; it will usually be more, it will never be less.
      if seconds_elapsed >= self._interval_seconds:
        total_seconds = int(round(now - self._start_time))
        self._SampleStats(tab, results, seconds=total_seconds)
        self._last_sample_time = now
    else:
      iterations_elapsed = self._iterations - self._last_sample_iterations
      if iterations_elapsed >= self._interval_iterations:
        self._SampleStats(tab, results, iterations=self._iterations)
        self._last_sample_iterations = self._iterations

  def _SampleStats(self, tab, results, seconds=None, iterations=None):
    """Records information and add it to the results."""

    def AddPoint(trace_name, units_y, value_y, chart_name=None):
      """Adds one data point to the results object."""
      if seconds:
        results.Add(trace_name + '_X', 'seconds', seconds,
                    data_type='unimportant', chart_name=chart_name)
      else:
        assert iterations, 'Neither seconds nor iterations given.'
        results.Add(trace_name + '_X', 'iterations', iterations,
                    data_type='unimportant', chart_name=chart_name)
      # Add the result as 'unimportant' because we want it to be unmonitored
      # by default.
      results.Add(trace_name + '_Y', units_y, value_y, data_type='unimportant',
                  chart_name=chart_name)
      # Save the value in a list so that summary stats can be calculated.
      if trace_name not in self._y_values:
        self._y_values[trace_name] = {
            'units': units_y,
            'chart_name': chart_name,
            'values': [],
        }
      self._y_values[trace_name]['values'].append(value_y)

    # DOM nodes and event listeners
    dom_stats = tab.dom_stats
    dom_node_count = dom_stats['node_count']
    event_listener_count = dom_stats['event_listener_count']
    AddPoint('dom_nodes', 'count', dom_node_count, chart_name='object_counts')
    AddPoint('event_listeners', 'count', event_listener_count,
             chart_name='object_counts')

    # Browser and renderer virtual memory stats
    memory_stats = self._browser.memory_stats
    def BrowserVMStats(statistic_name):
      """Get VM stats from the Browser object in KB."""
      return memory_stats[statistic_name].get('VM', 0) / 1024.0
    AddPoint('browser_vm', 'KB', BrowserVMStats('Browser'),
             chart_name='vm_stats')
    AddPoint('renderer_vm', 'KB', BrowserVMStats('Renderer'),
             chart_name='vm_stats')
    AddPoint('gpu_vm', 'KB', BrowserVMStats('Gpu'), chart_name='vm_stats')

    # V8 stats
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
    # Keep track of total test run length in terms of seconds and page
    # repetitions, so that test run length can be known regardless of
    # whether it is specified in seconds or iterations.
    results.AddSummary('total_iterations', 'iterations', self._iterations,
                       data_type='unimportant')
    results.AddSummary('total_time', 'seconds', time.time() - self._start_time,
                       data_type='unimportant')

    # Add summary stats which could be monitored for anomalies.
    for trace_name in self._y_values:
      units = self._y_values[trace_name]['units']
      chart_name = self._y_values[trace_name]['chart_name']
      values = self._y_values[trace_name]['values']
      results.AddSummary(trace_name + '_max', units, max(values),
                         chart_name=chart_name)


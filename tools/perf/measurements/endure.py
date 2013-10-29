# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import re
import time

from metrics import v8_object_stats
from telemetry.page import page_measurement

_V8_BYTES_COMMITTED = [
  'V8.MemoryNewSpaceBytesCommitted',
  'V8.MemoryOldPointerSpaceBytesCommitted',
  'V8.MemoryOldDataSpaceBytesCommitted',
  'V8.MemoryCodeSpaceBytesCommitted',
  'V8.MemoryMapSpaceBytesCommitted',
  'V8.MemoryCellSpaceBytesCommitted',
  'V8.MemoryPropertyCellSpaceBytesCommitted',
  'V8.MemoryLoSpaceBytesCommitted'
]
_V8_BYTES_USED = [
  'V8.MemoryNewSpaceBytesUsed',
  'V8.MemoryOldPointerSpaceBytesUsed',
  'V8.MemoryOldDataSpaceBytesUsed',
  'V8.MemoryCodeSpaceBytesUsed',
  'V8.MemoryMapSpaceBytesUsed',
  'V8.MemoryCellSpaceBytesUsed',
  'V8.MemoryPropertyCellSpaceBytesUsed',
  'V8.MemoryLoSpaceBytesUsed'
]
_V8_MEMORY_ALLOCATED = [
  'V8.OsMemoryAllocated'
]

class Endure(page_measurement.PageMeasurement):
  def __init__(self):
    super(Endure, self).__init__('endure')
    # Browser object, saved so that memory stats can be gotten later.
    self._browser = None

    # Timestamp for the time when the test starts.
    self._start_time = None
    # Timestamp of the last statistics sample.
    self._last_sample_time = 0

    # Number of page repetitions that have currently been done.
    self._iterations = 0
    # Number of page repetitions at the point of the last statistics sample.
    self._last_sample_iterations = 0

    # One of these variables will be set when the perf stats interval option
    # is parsed, and the other shall remain as None.
    self._interval_seconds = None
    self._interval_iterations = None

  def AddCommandLineOptions(self, parser):
    # TODO(tdu): When ProcessCommandLine is added to replace this method,
    # move the logic in _ParseIntervalOption there to ProcessCommandLine.
    group = optparse.OptionGroup(parser, 'Endure options')
    group.add_option('--perf-stats-interval',
                     dest='perf_stats_interval',
                     default='20s',
                     type='string',
                     help='Interval between sampling of statistics, either in '
                          'seconds (specified by appending \'s\') or in number '
                          'of iterations')
    parser.add_option_group(group)

  def DidStartBrowser(self, browser):
    # Save the Browser object so that memory_stats can be gotten later.
    self._browser = browser

  def CustomizeBrowserOptions(self, options):
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
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
    """Sample perf information if enough seconds or iterations have passed."""
    # Parse the interval option, setting either or seconds or iterations.
    # This is done here because self.options is not set when any of the above
    # methods are run.
    self._ParseIntervalOption()

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

  def _ParseIntervalOption(self):
    """Parse the perf stats interval option that was passed in."""
    if self._interval_seconds or self._interval_iterations:
      return
    interval = self.options.perf_stats_interval
    match = re.match('([0-9]+)([sS]?)$', interval)
    assert match, ('Invalid value for --perf-stats-interval: %s' % interval)
    if match.group(2):
      self._interval_seconds = int(match.group(1))
    else:
      self._interval_iterations = int(match.group(1))
    assert self._interval_seconds or self._interval_iterations

  def _SampleStats(self, tab, results, seconds=None, iterations=None):
    """Record memory information and add it to the results."""

    def AddPoint(trace_name, units_y, value_y):
      """Add one data point to the results object."""
      if seconds:
        results.Add(trace_name + '_X', 'seconds', seconds)
      else:
        assert iterations, 'Neither seconds nor iterations given.'
        results.Add(trace_name + '_X', 'iterations', iterations)
      results.Add(trace_name + '_Y', units_y, value_y)

    # DOM nodes and event listeners
    dom_stats = tab.dom_stats
    dom_node_count = dom_stats['node_count']
    event_listener_count = dom_stats['event_listener_count']
    AddPoint('dom_nodes', 'count', dom_node_count)
    AddPoint('event_listeners', 'count', event_listener_count)

    # Browser and renderer virtual memory stats
    memory_stats = self._browser.memory_stats
    def BrowserVMStats(statistic_name):
      """Get VM stats from the Browser object in KB."""
      return memory_stats[statistic_name].get('VM', 0) / 1024.0
    AddPoint('browser_vm', 'KB', BrowserVMStats('Browser'))
    AddPoint('renderer_vm', 'KB', BrowserVMStats('Renderer'))
    AddPoint('gpu_vm', 'KB', BrowserVMStats('Gpu'))

    # V8 stats
    def V8StatsSum(counters):
      """Given a list of V8 counter names, get the sum of the values in KB."""
      stats = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(tab, counters)
      return sum(stats.values()) / 1024.0
    AddPoint('v8_memory_committed', 'KB', V8StatsSum(_V8_BYTES_COMMITTED))
    AddPoint('v8_memory_used', 'KB', V8StatsSum(_V8_BYTES_USED))
    AddPoint('v8_memory_allocated', 'KB', V8StatsSum(_V8_MEMORY_ALLOCATED))


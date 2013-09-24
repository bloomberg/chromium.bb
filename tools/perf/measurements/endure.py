# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import v8_object_stats
from telemetry.page import page_measurement

import optparse
import time

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

  def AddCommandLineOptions(self, parser):
    group = optparse.OptionGroup(parser, 'Endure options')
    group.add_option('--perf-stats-interval',
                     dest='perf_stats_interval',
                     default=20,
                     type='int',
                     help='Time interval between perf dumps (secs)')
    parser.add_option_group(group)

  def DidStartBrowser(self, browser):
    # Save the Browser object so that memory_stats can be gotten later.
    self._browser = browser

  def CustomizeBrowserOptions(self, options):
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'endure')

  def WillRunPageRepeats(self, page, tab):
    """Reset the starting time for each new page."""
    self._start_time = time.time()

    # Prefix the page name so it can be picked up by the buildbot script that
    # parses Endure output.
    if page.name and not page.display_name.startswith('endure_'):
      page.name = 'endure_' + page.name

  def MeasurePage(self, page, tab, results):
    """Dump perf information if we have gone past our interval time."""
    now = time.time()
    seconds_elapsed = int(round(now - self._last_sample_time))
    if seconds_elapsed > self.options.perf_stats_interval:
      self._last_sample_time = now
      self._SampleStats(tab, results, now)

  def _SampleStats(self, tab, results, now):
    """Record memory information and add it to the results."""
    elapsed_time = int(round(now - self._start_time))

    def AddPoint(trace_name, units_y, value_y):
      """Add one data point to the results object."""
      results.Add(trace_name + '_X', 'seconds', elapsed_time)
      results.Add(trace_name + '_Y', units_y, value_y)

    # DOM nodes and event listeners
    dom_stats = tab.dom_stats
    dom_node_count = dom_stats['node_count']
    event_listener_count = dom_stats['event_listener_count']
    AddPoint('TotalDOMNodeCount', 'nodes', dom_node_count)
    AddPoint('EventListenerCount', 'events', event_listener_count)

    # Browser and renderer virtual memory stats
    memory_stats = self._browser.memory_stats
    def BrowserVMStats(statistic_name):
      """Get VM stats from the Browser object in KB."""
      return memory_stats[statistic_name].get('VM', 0) / 1024.0
    AddPoint('BrowserVirtualMemory', 'KB', BrowserVMStats('Browser'))
    AddPoint('RendererVirtualMemory', 'KB', BrowserVMStats('Renderer'))

    # V8 stats
    def V8StatsSum(counters):
      """Given a list of V8 counter names, get the sum of the values in KB."""
      stats = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(tab, counters)
      return sum(stats.values()) / 1024.0
    AddPoint('V8BytesCommitted', 'KB', V8StatsSum(_V8_BYTES_COMMITTED))
    AddPoint('V8BytesUsed', 'KB', V8StatsSum(_V8_BYTES_USED))
    AddPoint('V8MemoryAllocated', 'KB', V8StatsSum(_V8_MEMORY_ALLOCATED))


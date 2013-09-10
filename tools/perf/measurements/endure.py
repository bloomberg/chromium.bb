# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import v8_object_stats
from telemetry.page import page_measurement

import optparse
import time

_V8_BYTES_COMMITED = [
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
    self._browser = None
    self._test_start_time = None

    # Timestamp of the last memory retrieval.
    self._last_mem_dump = 0

  def AddCommandLineOptions(self, parser):
    group = optparse.OptionGroup(parser, 'Endure options')
    group.add_option('--perf-stats-interval',
                     dest='perf_stats_interval',
                     default=20,
                     type='int',
                     help='Time interval between perf dumps (secs)')
    parser.add_option_group(group)

  def DidStartBrowser(self, browser):
    # Save the browser for memory_stats.
    self._browser = browser

  def CustomizeBrowserOptions(self, options):
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'endure')

  def WillRunPageRepeats(self, page, tab):
    """Reset the starting time for each new page."""
    self._test_start_time = time.time()

    # Prefix the page name so it can be picked up by endure parser.
    if page.name and not page.display_name.startswith('endure_'):
      page.name = 'endure_' + page.name

  def MeasurePage(self, page, tab, results):
    """Dump perf information if we have gone past our interval time."""
    now = time.time()
    if int(round(now - self._last_mem_dump)) > self.options.perf_stats_interval:
      self._last_mem_dump = now
      self._GetPerformanceStats(tab, results, now)

  def _GetPerformanceStats(self, tab, results, now):
    """Record all memory information."""
    elapsed_time = int(round(now - self._test_start_time))

    # DOM Nodes
    dom_stats = tab.dom_stats
    dom_node_count = dom_stats['node_count']
    self._SaveToResults(results, 'TotalDOMNodeCount_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'TotalDOMNodeCount_Y',
                        'nodes', dom_node_count)

    # Event Listeners
    event_listener_count = dom_stats['event_listener_count']
    self._SaveToResults(results, 'EventListenerCount_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'EventListenerCount_Y',
                        'listeners', event_listener_count)

    # Memory stats
    memory_stats = self._browser.memory_stats
    browser_vm = memory_stats['Browser'].get('VM', 0) / 1024.0
    self._SaveToResults(results, 'BrowserVirtualMemory_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'BrowserVirtualMemory_Y',
                        'KB', browser_vm)
    renderer_vm = memory_stats['Renderer'].get('VM', 0) / 1024.0
    self._SaveToResults(results, 'RendererVirtualMemory_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'RendererVirtualMemory_Y',
                        'KB', renderer_vm)

    # V8 stats
    v8_bytes_commited = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(
                            tab, _V8_BYTES_COMMITED)
    v8_bytes_commited = sum(v8_bytes_commited.values()) / 1024.0
    self._SaveToResults(results, 'V8BytesCommited_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'V8BytesCommited_Y',
                        'KB', v8_bytes_commited)

    v8_bytes_used = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(
                            tab, _V8_BYTES_USED)
    print v8_bytes_used
    v8_bytes_used = sum(v8_bytes_used.values()) / 1024.0
    self._SaveToResults(results, 'V8BytesUsed_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'V8BytesUsed_Y',
                        'KB', v8_bytes_used)

    v8_mem_allocated = v8_object_stats.V8ObjectStatsMetric.GetV8StatsTable(
                            tab, _V8_MEMORY_ALLOCATED)
    v8_mem_allocated = sum(v8_mem_allocated.values()) / 1024.0
    self._SaveToResults(results, 'V8MemoryAllocated_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'V8MemoryAllocated_Y',
                        'KB', v8_mem_allocated)

  def _SaveToResults(self, results, trace_name, units, value,
                     chart_name=None, data_type='default'):
    results.Add(trace_name, units, value, chart_name, data_type)

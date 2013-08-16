# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_measurement

import optparse
import time


class Endure(page_measurement.PageMeasurement):
  def __init__(self):
    super(Endure, self).__init__('endure')
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

  def CanRunForPage(self, page):
    return hasattr(page, 'endure')

  def WillRunPageRepeats(self, page, tab):
    """Reset the starting time for each new page."""
    self._test_start_time = time.time()

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
    dom_node_count = tab.dom_stats['node_count']
    self._SaveToResults(results, 'TotalDOMNodeCount_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'TotalDOMNodeCount_Y',
                        'nodes', dom_node_count)

    # Event Listeners
    event_listener_count = tab.dom_stats['event_listener_count']
    self._SaveToResults(results, 'EventListenerCount_X',
                        'seconds', elapsed_time)
    self._SaveToResults(results, 'EventListenerCount_Y',
                        'listeners', event_listener_count)

  def _SaveToResults(self, results, trace_name, units, value,
                     chart_name=None, data_type='default'):
    results.Add(trace_name, units, value, chart_name, data_type)

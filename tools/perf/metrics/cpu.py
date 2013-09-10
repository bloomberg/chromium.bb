# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric

class Cpu(Metric):
  """Calulates CPU load over a span of time."""

  def __init__(self, browser):
    super(Cpu, self).__init__()
    self._results = None
    self._browser = browser
    self.start_cpu = None

  def DidStartBrowser(self, browser):
    # Save the browser for cpu_stats.
    self._browser = browser

  def Start(self, page, tab):
    self.start_cpu = self._browser.cpu_stats

  def Stop(self, page, tab):
    assert self.start_cpu != None, 'Must call Start() first'
    self._results = Cpu.SubtractCpuStats(self._browser.cpu_stats,
                                         self.start_cpu)

  # pylint: disable=W0221
  def AddResults(self, tab, results, trace_name='cpu_utilization'):
    assert self._results != None, 'Must call Stop() first'
    for process_type in self._results:
      results.Add(trace_name + '_%s' % process_type.lower(),
                  '%',
                  self._results[process_type]['CpuPercent'],
                  chart_name='cpu_utilization',
                  data_type='unimportant')

  @staticmethod
  def SubtractCpuStats(cpu_stats, start_cpu_stats):
    """Returns difference of a previous cpu_stats from a cpu_stats."""
    diff = {}
    for process_type in cpu_stats:
      assert process_type in start_cpu_stats, 'Mismatching process types'
      # Skip any process_types that are empty.
      if (not len(cpu_stats[process_type]) or
          not len(start_cpu_stats[process_type])):
        continue
      cpu_process_time = (cpu_stats[process_type]['CpuProcessTime'] -
                          start_cpu_stats[process_type]['CpuProcessTime'])
      total_time = (cpu_stats[process_type]['TotalTime'] -
                    start_cpu_stats[process_type]['TotalTime'])
      assert total_time > 0
      cpu_percent = 100 * cpu_process_time / total_time
      diff[process_type] = {'CpuPercent': cpu_percent}
    return diff

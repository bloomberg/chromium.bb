# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from telemetry.value import scalar

import metrics


class SystemMemoryMetric(metrics.Metric):
  """SystemMemoryMetric gathers system memory statistic.

  This metric collects system memory stats per test.  It reports the difference
  (delta) in system memory starts from the start of the test to the end of it.
  """

  def __init__(self, browser):
    super(SystemMemoryMetric, self).__init__()
    self._browser = browser
    self._memory_stats_start = None
    self._memory_stats_end = None

  def Start(self, page, tab):
    """Start the per-page preparation for this metric.

    Records the system memory stats at this point.
    """
    self._memory_stats_start = self._browser.memory_stats

  def Stop(self, page, tab):
    """Prepare the results for this page.

    The results are the differences between the current system memory stats
    and the values when Start() was called.
    """
    assert self._memory_stats_start, 'Must call Start() first'
    self._memory_stats_end = self._browser.memory_stats

  # |trace_name| and |exclude_metrics| args are not in base class Metric.
  # pylint: disable=arguments-differ
  def AddResults(self, tab, results, trace_name=None, exclude_metrics=None):
    """Add results for this page to the results object.

    Reports the delta in memory stats between the start stats and the end stats
    (as *_delta metrics). It reports end memory stats in case no matching start
    memory stats exists.

    Args:
      trace_name: Trace name to identify the summary results for current page.
      exclude_metrics: List of memory metrics to exclude from results,
                       e.g. VM, VMPeak, etc. See _AddResultsForProcesses().
    """
    assert self._memory_stats_end, 'Must call Stop() first'
    memory_stats = _SubtractMemoryStats(self._memory_stats_end,
                                        self._memory_stats_start)
    if not memory_stats['Browser']:
      return
    exclude_metrics = exclude_metrics or {}
    _AddResultsForProcesses(
        results, memory_stats,
        metric_trace_name=trace_name, chart_trace_name='delta',
        exclude_metrics=exclude_metrics)

    if 'SystemCommitCharge' not in exclude_metrics:
      results.AddValue(scalar.ScalarValue(
          results.current_page,
          'commit_charge_delta.%s' % (trace_name or 'commit_charge'), 'kb',
          memory_stats['SystemCommitCharge'], important=False))

    if 'ProcessCount' not in exclude_metrics:
      results.AddValue(scalar.ScalarValue(
          results.current_page,
          'processes_delta.%s' % (trace_name or 'processes'), 'count',
          memory_stats['ProcessCount'], important=False))


def _SubtractMemoryStats(end_memory_stats, start_memory_stats):
  """Computes the difference in memory usage stats.

  Each of the two stats arguments is a dict with the following format:
      {'Browser': {metric: value, ...},
       'Renderer': {metric: value, ...},
       'Gpu': {metric: value, ...},
       'ProcessCount': value,
       etc
      }
  The metrics can be VM, WorkingSetSize, ProportionalSetSize, etc depending on
  the platform/test.

  NOTE: The only metrics that are not subtracted from original are the *Peak*
  memory values.

  Returns:
    A dict of process type names (Browser, Renderer, etc.) to memory usage
    metrics between the end collected stats and the start collected stats.
  """
  memory_stats = {}
  end_memory_stats = end_memory_stats or {}
  start_memory_stats = start_memory_stats or {}

  for process_type in end_memory_stats:
    memory_stats[process_type] = {}
    end_process_memory = end_memory_stats[process_type]
    if not end_process_memory:
      continue

    # If a process has end stats without start stats then report the end stats.
    # For example, a GPU process that started just after media playback.
    if (process_type not in start_memory_stats or
        not start_memory_stats[process_type]):
      memory_stats[process_type] = end_process_memory
      continue

    if not isinstance(end_process_memory, dict):
      start_value = start_memory_stats[process_type] or 0
      memory_stats[process_type] = end_process_memory - start_value
    else:
      for metric in end_process_memory:
        end_value = end_process_memory[metric]
        start_value = start_memory_stats[process_type].get(metric, 0)
        if 'Peak' in metric:
          memory_stats[process_type][metric] = end_value
        else:
          memory_stats[process_type][metric] = end_value - start_value
  return memory_stats


def _AddResultsForProcesses(results, memory_stats, chart_trace_name='final',
                           metric_trace_name=None,
                           exclude_metrics=None):
  """Adds memory stats for browser, renderer and gpu processes.

  Args:
    results: A telemetry.results.PageTestResults object.
    memory_stats: System memory stats collected.
    chart_trace_name: Trace to identify memory metrics. Default is 'final'.
    metric_trace_name: Trace to identify the metric results per test page.
    exclude_metrics: List of memory metrics to exclude from results,
                     e.g. VM, WorkingSetSize, etc.
  """
  metric = 'resident_set_size'
  if sys.platform == 'win32':
    metric = 'working_set'

  exclude_metrics = exclude_metrics or {}

  def AddResultsForProcessTypes(process_types_memory, process_type_trace):
    """Add all results for a given set of process types.

    Args:
      process_types_memory: A list of process types, e.g. Browser, 'Renderer'.
      process_type_trace: The name of this set of process types in the output.
    """
    def AddResult(value_name_memory, value_name_trace, description):
      """Add a result for a given statistic.

      Args:
        value_name_memory: Name of some statistic, e.g. VM, WorkingSetSize.
        value_name_trace: Name of this statistic to be used in the output.
      """
      if value_name_memory in exclude_metrics:
        return
      if len(process_types_memory) > 1 and value_name_memory.endswith('Peak'):
        return
      values = []
      for process_type_memory in process_types_memory:
        stats = memory_stats[process_type_memory]
        if value_name_memory in stats:
          values.append(stats[value_name_memory])
      if values:
        if metric_trace_name:
          current_trace = '%s_%s' % (metric_trace_name, process_type_trace)
          chart_name = value_name_trace
        else:
          current_trace = '%s_%s' % (value_name_trace, process_type_trace)
          chart_name = current_trace
        results.AddValue(scalar.ScalarValue(
            results.current_page, '%s.%s' % (chart_name, current_trace), 'kb',
            sum(values) / 1024, important=False, description=description))

    AddResult('VM', 'vm_%s_size' % chart_trace_name,
              'Virtual Memory Size (address space allocated).')
    AddResult('WorkingSetSize', 'vm_%s_%s_size' % (metric, chart_trace_name),
              'Working Set Size (Windows) or Resident Set Size (other '
              'platforms).')
    AddResult('PrivateDirty', 'vm_private_dirty_%s' % chart_trace_name,
              'Private Dirty is basically the amount of RAM inside the '
              'process that can not be paged to disk (it is not backed by the '
              'same data on disk), and is not shared with any other '
              'processes. Another way to look at this is the RAM that will '
              'become available to the system when that process goes away '
              '(and probably quickly subsumed into caches and other uses of '
              'it).')
    AddResult('ProportionalSetSize',
              'vm_proportional_set_size_%s' % chart_trace_name,
              'The Proportional Set Size (PSS) number is a metric the kernel '
              'computes that takes into account memory sharing -- basically '
              'each page of RAM in a process is scaled by a ratio of the '
              'number of other processes also using that page. This way you '
              'can (in theory) add up the PSS across all processes to see '
              'the total RAM they are using, and compare PSS between '
              'processes to get a rough idea of their relative weight.')
    AddResult('SharedDirty', 'vm_shared_dirty_%s' % chart_trace_name,
              'Shared Dirty is the amount of RAM outside the process that can '
              'not be paged to disk, and is shared with other processes.')
    AddResult('VMPeak', 'vm_peak_size',
              'The peak Virtual Memory Size (address space allocated) usage '
              'achieved by the * process.')
    AddResult('WorkingSetSizePeak', '%s_peak_size' % metric,
              'Peak Working Set Size.')

  AddResultsForProcessTypes(['Browser'], 'browser')
  AddResultsForProcessTypes(['Renderer'], 'renderer')
  AddResultsForProcessTypes(['Gpu'], 'gpu')
  AddResultsForProcessTypes(['Browser', 'Renderer', 'Gpu'], 'total')

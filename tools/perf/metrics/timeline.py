# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

from metrics import Metric

TRACING_MODE = 'tracing-mode'
TIMELINE_MODE = 'timeline-mode'

class TimelineMetric(Metric):
  def __init__(self, mode):
    ''' Initializes a TimelineMetric object.

    mode: TRACING_MODE or TIMELINE_MODE
    thread_filter: list of thread names to include.
        Empty list or None includes all threads.
        In TIMELINE_MODE, only 'thread 0' is available, which is the renderer
        main thread.
        In TRACING_MODE, the following renderer process threads are available:
        'CrRendererMain', 'Chrome_ChildIOThread', and 'Compositor'.
        CompositorRasterWorker threads are available with impl-side painting
        enabled.
    '''
    assert mode in (TRACING_MODE, TIMELINE_MODE)
    super(TimelineMetric, self).__init__()
    self._mode = mode
    self._model = None

  def Start(self, page, tab):
    self._model = None
    if self._mode == TRACING_MODE:
      if not tab.browser.supports_tracing:
        raise Exception('Not supported')
      tab.browser.StartTracing()
    else:
      assert self._mode == TIMELINE_MODE
      tab.StartTimelineRecording()

  def Stop(self, page, tab):
    if self._mode == TRACING_MODE:
      trace_result = tab.browser.StopTracing()
      self._model = trace_result.AsTimelineModel()
    else:
      tab.StopTimelineRecording()
      self._model = tab.timeline_model

  def GetRendererProcess(self, tab):
    if self._mode == TRACING_MODE:
      return self._model.GetRendererProcessFromTab(tab)
    else:
      return self._model.GetAllProcesses()[0]

  def AddResults(self, tab, results):
    return


class LoadTimesTimelineMetric(TimelineMetric):
  def __init__(self, mode, thread_filter = None):
    super(LoadTimesTimelineMetric, self).__init__(mode)
    self._thread_filter = thread_filter

  def AddResults(self, tab, results):
    assert self._model

    renderer_process = self.GetRendererProcess(tab)
    events_by_name = collections.defaultdict(list)

    for thread in renderer_process.threads.itervalues():

      if self._thread_filter and not thread.name in self._thread_filter:
        continue

      thread_name = thread.name.replace('/','_')
      events = thread.all_slices

      for e in events:
        events_by_name[e.name].append(e)

      for event_name, event_group in events_by_name.iteritems():
        times = [event.self_time for event in event_group]
        total = sum(times)
        biggest_jank = max(times)
        full_name = thread_name + '|' + event_name
        results.Add(full_name, 'ms', total)
        results.Add(full_name + '_max', 'ms', biggest_jank)
        results.Add(full_name + '_avg', 'ms', total / len(times))

    for counter_name, counter in renderer_process.counters.iteritems():
      total = sum(counter.totals)
      results.Add(counter_name, 'count', total)
      results.Add(counter_name + '_avg', 'count', total / len(counter.totals))


# We want to generate a consistant picture of our thread usage, despite
# having several process configurations (in-proc-gpu/single-proc).
# Since we can't isolate renderer threads in single-process mode, we
# always sum renderer-process threads' times. We also sum all io-threads
# for simplicity.
TimelineThreadCategories =  {
  # These are matched exactly
  "Chrome_InProcGpuThread": "GPU",
  "CrGPUMain"             : "GPU",
  "AsyncTransferThread"   : "GPU_transfer",
  "CrBrowserMain"         : "browser_main",
  "Browser Compositor"    : "browser_compositor",
  "CrRendererMain"        : "renderer_main",
  "Compositor"            : "renderer_compositor",
  # These are matched by substring
  "IOThread"              : "IO",
  "CompositorRasterWorker": "raster"
}

def ThreadTimePercentageName(category):
  return "thread_" + category + "_clock_time_percentage"

def ThreadCPUTimePercentageName(category):
  return "thread_" + category + "_cpu_time_percentage"

class ThreadTimesTimelineMetric(TimelineMetric):
  def __init__(self):
    super(ThreadTimesTimelineMetric, self).__init__(TRACING_MODE)

  def AddResults(self, tab, results):
    # Default each category to zero for consistant results.
    category_clock_times = collections.defaultdict(float)
    category_cpu_times = collections.defaultdict(float)

    for category in TimelineThreadCategories.values():
      category_clock_times[category] = 0
      category_cpu_times[category] = 0

    # Add up thread time for all threads we care about.
    for thread in self._model.GetAllThreads():
      # First determine if we care about this thread.
      # Check substrings first, followed by exact matches
      thread_category = None
      for substring, category in TimelineThreadCategories.iteritems():
        if substring in thread.name:
          thread_category = category
      if thread.name in TimelineThreadCategories:
        thread_category = TimelineThreadCategories[thread.name]
      if thread_category == None:
        thread_category = "other"

      # Sum and add top-level slice durations
      clock = sum([event.duration for event in thread.toplevel_slices])
      category_clock_times[thread_category] += clock
      cpu = sum([event.thread_duration for event in thread.toplevel_slices])
      category_cpu_times[thread_category] += cpu

    # Now report each category. We report the percentage of time that
    # the thread is running rather than absolute time, to represent how
    # busy the thread is. This needs to be interpretted when throughput
    # is changed due to scheduling changes (eg. more frames produced
    # in the same time period). It would be nice if we could correct
    # for that somehow.
    for category, category_time in category_clock_times.iteritems():
      report_name = ThreadTimePercentageName(category)
      time_as_percentage = (category_time / self._model.bounds.bounds) * 100
      results.Add(report_name, '%', time_as_percentage)

    # Do the same for CPU (scheduled) time.
    for category, category_time in category_cpu_times.iteritems():
      report_name = ThreadCPUTimePercentageName(category)
      time_as_percentage = (category_time / self._model.bounds.bounds) * 100
      results.Add(report_name, '%', time_as_percentage)

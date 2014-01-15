# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

from metrics import Metric

TRACING_MODE = 'tracing-mode'
TIMELINE_MODE = 'timeline-mode'

class TimelineMetric(Metric):
  def __init__(self, mode):
    """ Initializes a TimelineMetric object.
    """
    super(TimelineMetric, self).__init__()
    assert mode in (TRACING_MODE, TIMELINE_MODE)
    self._mode = mode
    self._model = None
    self._renderer_process = None

  def Start(self, page, tab):
    """Starts gathering timeline data.

    mode: TRACING_MODE or TIMELINE_MODE
    """
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
      self._renderer_process = self._model.GetRendererProcessFromTab(tab)
    else:
      tab.StopTimelineRecording()
      self._model = tab.timeline_model
      self._renderer_process = self._model.GetAllProcesses()[0]

  @property
  def model(self):
    return self._model

  @model.setter
  def model(self, model):
    self._model = model

  @property
  def renderer_process(self):
    return self._renderer_process

  def AddResults(self, tab, results):
    return

class LoadTimesTimelineMetric(TimelineMetric):
  def __init__(self, mode):
    super(LoadTimesTimelineMetric, self).__init__(mode)
    self.report_main_thread_only = True

  def AddResults(self, _, results):
    assert self._model
    if self.report_main_thread_only:
      if self._mode == TRACING_MODE:
        thread_filter = 'CrRendererMain'
      else:
        thread_filter = 'thread 0'
    else:
      thread_filter = None

    events_by_name = collections.defaultdict(list)

    for thread in self.renderer_process.threads.itervalues():

      if thread_filter and not thread.name in thread_filter:
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

    for counter_name, counter in self.renderer_process.counters.iteritems():
      total = sum(counter.totals)
      results.Add(counter_name, 'count', total)
      results.Add(counter_name + '_avg', 'count', total / len(counter.totals))


# We want to generate a consistant picture of our thread usage, despite
# having several process configurations (in-proc-gpu/single-proc).
# Since we can't isolate renderer threads in single-process mode, we
# always sum renderer-process threads' times. We also sum all io-threads
# for simplicity.
TimelineThreadCategories =  {
  "Chrome_InProcGpuThread": "GPU",
  "CrGPUMain"             : "GPU",
  "AsyncTransferThread"   : "GPU_transfer",
  "CrBrowserMain"         : "browser_main",
  "Browser Compositor"    : "browser_compositor",
  "CrRendererMain"        : "renderer_main",
  "Compositor"            : "renderer_compositor",
  "IOThread"              : "IO",
  "CompositorRasterWorker": "raster"
}
MatchBySubString = ["IOThread", "CompositorRasterWorker"]
FastPath = ["GPU",
            "browser_main",
            "browser_compositor",
            "renderer_compositor",
            "IO"]


def ThreadTimePercentageName(category):
  return "thread_" + category + "_clock_time_percentage"

def ThreadCPUTimePercentageName(category):
  return "thread_" + category + "_cpu_time_percentage"

class ResultsForThread(object):
  def __init__(self, model):
    self.model = model
    self.toplevel_slices = []
    self.all_slices = []

  @property
  def clock_time(self):
    return sum([x.duration for x in self.toplevel_slices])

  @property
  def cpu_time(self):
    res = 0
    for x in self.toplevel_slices:
      # Only report thread-duration if we have it for all events.
      #
      # A thread_duration of 0 is valid, so this only returns 0 if it is None.
      if x.thread_duration == None:
        return 0
      else:
        res += x.thread_duration
    return res

  def AddDetailedResults(self, thread_category_name, results):
    slices_by_category = collections.defaultdict(list)
    for s in self.all_slices:
      slices_by_category[s.category].append(s)
    all_self_times = []
    for category, slices_in_category in slices_by_category.iteritems():
      self_time = sum([x.self_time for x in slices_in_category])
      results.Add('%s|%s' % (thread_category_name, category), 'ms', self_time)
      all_self_times.append(self_time)
    all_measured_time = sum(all_self_times)
    idle_time = max(0,
                    self.model.bounds.bounds - all_measured_time)
    results.Add('%s|idle' % thread_category_name, 'ms', idle_time)

class ThreadTimesTimelineMetric(TimelineMetric):
  def __init__(self):
    super(ThreadTimesTimelineMetric, self).__init__(TRACING_MODE)
    self.report_renderer_main_details = False

  def GetThreadCategoryName(self, thread):
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

    return thread_category

  def AddResults(self, tab, results):
    results_per_thread_category = collections.defaultdict(
        lambda: ResultsForThread(self._model))

    # Set up each category anyway so that we get consistant results.
    for category in TimelineThreadCategories.values():
      results_per_thread_category[category] = ResultsForThread(self.model)
    results_for_all_threads = results_per_thread_category['total_fast_path']

    # Group the slices by their thread category.
    for thread in self._model.GetAllThreads():
      # First determine if we care about this thread.
      # Check substrings first, followed by exact matches
      thread_category = self.GetThreadCategoryName(thread)

      results_for_thread = results_per_thread_category[thread_category]
      for event in thread.all_slices:
        results_for_thread.all_slices.append(event)
        results_for_all_threads.all_slices.append(event)
      for event in thread.toplevel_slices:
        results_for_thread.toplevel_slices.append(event)
        results_for_all_threads.toplevel_slices.append(event)

    for thread_category, results_for_thread_category in (
        results_per_thread_category.iteritems()):
      thread_report_name = ThreadTimePercentageName(thread_category)
      time_as_percentage = (float(results_for_thread_category.clock_time) /
                            self._model.bounds.bounds) * 100
      results.Add(thread_report_name, '%', time_as_percentage)

    for thread_category, results_for_thread_category in (
        results_per_thread_category.iteritems()):
      cpu_time_report_name = ThreadCPUTimePercentageName(thread_category)
      time_as_percentage = (float(results_for_thread_category.cpu_time) /
                            self._model.bounds.bounds) * 100
      results.Add(cpu_time_report_name, '%', time_as_percentage)

    # TOOD(nduca): When generic results objects are done, this special case
    # can be replaced with a generic UI feature.
    for thread_category, results_for_thread_category in (
        results_per_thread_category.iteritems()):
      if (thread_category == 'renderer_main' and
          self.report_renderer_main_details):
        results_for_thread_category.AddDetailedResults(thread_category, results)

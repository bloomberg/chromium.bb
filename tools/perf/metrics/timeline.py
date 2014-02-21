# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import itertools

from metrics import Metric
from telemetry.core.timeline import bounds
from telemetry.page import page_measurement

TRACING_MODE = 'tracing-mode'
TIMELINE_MODE = 'timeline-mode'

# All tracing categories not disabled-by-default
DEFAULT_TRACE_CATEGORIES = None

# Categories for absolute minimum overhead tracing. This contains no
# sub-traces of thread tasks, so it's only useful for capturing the
# cpu-time spent on threads (as well as needed benchmark traces)
MINIMAL_TRACE_CATEGORIES = ("toplevel,"
                            "benchmark,"
                            "webkit.console,"
                            "trace_event_overhead")

class MissingFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(MissingFramesError, self).__init__(
        'No frames found in trace. Unable to normalize results.')


class TimelineMetric(Metric):
  def __init__(self, mode):
    """ Initializes a TimelineMetric object.
    """
    super(TimelineMetric, self).__init__()
    assert mode in (TRACING_MODE, TIMELINE_MODE)
    self.trace_categories = DEFAULT_TRACE_CATEGORIES
    self._mode = mode
    self._model = None
    self._renderer_process = None
    self._actions = []
    self._action_ranges = []

  def Start(self, page, tab):
    """Starts gathering timeline data.

    mode: TRACING_MODE or TIMELINE_MODE
    """
    self._model = None
    if self._mode == TRACING_MODE:
      if not tab.browser.supports_tracing:
        raise Exception('Not supported')
      tab.browser.StartTracing(self.trace_categories)
    else:
      assert self._mode == TIMELINE_MODE
      tab.StartTimelineRecording()

  def Stop(self, page, tab):
    if self._mode == TRACING_MODE:
      trace_result = tab.browser.StopTracing()
      self._model = trace_result.AsTimelineModel()
      self._renderer_process = self._model.GetRendererProcessFromTab(tab)
      self._action_ranges = [ action.GetActiveRangeOnTimeline(self._model)
                              for action in self._actions ]
      # Make sure no action ranges overlap
      for combo in itertools.combinations(self._action_ranges, 2):
        assert not combo[0].Intersects(combo[1])
    else:
      tab.StopTimelineRecording()
      self._model = tab.timeline_model
      self._renderer_process = self._model.GetAllProcesses()[0]

  def AddActionToIncludeInMetric(self, action):
    self._actions.append(action)

  @property
  def model(self):
    return self._model

  @model.setter
  def model(self, model):
    self._model = model

  @property
  def renderer_process(self):
    return self._renderer_process

  @renderer_process.setter
  def renderer_process(self, p):
    self._renderer_process = p

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

        # Results objects cannot contain the '.' character, so remove that here.
        sanitized_event_name = event_name.replace('.', '_')

        full_name = thread_name + '|' + sanitized_event_name
        results.Add(full_name, 'ms', total)
        results.Add(full_name + '_max', 'ms', biggest_jank)
        results.Add(full_name + '_avg', 'ms', total / len(times))

    for counter_name, counter in self.renderer_process.counters.iteritems():
      total = sum(counter.totals)

      # Results objects cannot contain the '.' character, so remove that here.
      sanitized_counter_name = counter_name.replace('.', '_')

      results.Add(sanitized_counter_name, 'count', total)
      results.Add(sanitized_counter_name + '_avg', 'count',
                  total / float(len(counter.totals)))


# We want to generate a consistant picture of our thread usage, despite
# having several process configurations (in-proc-gpu/single-proc).
# Since we can't isolate renderer threads in single-process mode, we
# always sum renderer-process threads' times. We also sum all io-threads
# for simplicity.
TimelineThreadCategories =  {
  "Chrome_InProcGpuThread": "GPU",
  "CrGpuMain"             : "GPU",
  "AsyncTransferThread"   : "GPU_transfer",
  "CrBrowserMain"         : "browser",
  "Browser Compositor"    : "browser",
  "CrRendererMain"        : "renderer_main",
  "Compositor"            : "renderer_compositor",
  "IOThread"              : "IO",
  "CompositorRasterWorker": "raster",
  "DummyThreadName1"      : "other",
  "DummyThreadName2"      : "total_fast_path",
  "DummyThreadName3"      : "total_all"
}

_MatchBySubString = ["IOThread", "CompositorRasterWorker"]

AllThreads = TimelineThreadCategories.values()
NoThreads = []
FastPathThreads = ["GPU", "renderer_compositor", "browser", "IO"]

ReportMainThreadOnly = ["renderer_main"]
ReportFastPathResults = AllThreads
ReportFastPathDetails = NoThreads
ReportSilkResults = ["renderer_main", "total_all"]
ReportSilkDetails = ["renderer_main"]

# TODO(epenner): Thread names above are likely fairly stable but trace names
# could change. We should formalize these traces to keep this robust.
OverheadTraceCategory = "trace_event_overhead"
OverheadTraceName = "overhead"
FrameTraceName = "::SwapBuffers"
FrameTraceThreadName = "renderer_compositor"


def ClockOverheadForEvent(event):
  if (event.category == OverheadTraceCategory and
      event.name == OverheadTraceName):
    return event.duration
  else:
    return 0

def CpuOverheadForEvent(event):
  if (event.category == OverheadTraceCategory and
      event.thread_duration):
    return event.thread_duration
  else:
    return 0

def ThreadCategoryName(thread_name):
  thread_category = "other"
  for substring, category in TimelineThreadCategories.iteritems():
    if substring in _MatchBySubString and substring in thread_name:
      thread_category = category
  if thread_name in TimelineThreadCategories:
    thread_category = TimelineThreadCategories[thread_name]
  return thread_category

def ThreadTimeResultName(thread_category):
  return "thread_" + thread_category + "_clock_time_per_frame"

def ThreadCpuTimeResultName(thread_category):
  return "thread_" + thread_category + "_cpu_time_per_frame"

def ThreadDetailResultName(thread_category, detail):
  detail_sanitized = detail.replace('.','_')
  return "thread_" + thread_category + "|" + detail_sanitized


class ResultsForThread(object):
  def __init__(self, model, action_ranges, name):
    self.model = model
    self.toplevel_slices = []
    self.all_slices = []
    self.name = name
    self.action_ranges = action_ranges

  @property
  def clock_time(self):
    clock_duration = sum([x.duration for x in self.toplevel_slices])
    clock_overhead = sum([ClockOverheadForEvent(x) for x in self.all_slices])
    return clock_duration - clock_overhead

  @property
  def cpu_time(self):
    cpu_duration = 0
    cpu_overhead = sum([CpuOverheadForEvent(x) for x in self.all_slices])
    for x in self.toplevel_slices:
      # Only report thread-duration if we have it for all events.
      #
      # A thread_duration of 0 is valid, so this only returns 0 if it is None.
      if x.thread_duration == None:
        if not x.duration:
          continue
        else:
          return 0
      else:
        cpu_duration += x.thread_duration
    return cpu_duration - cpu_overhead

  def SlicesInActions(self, slices):
    slices_in_actions = []
    for event in slices:
      for action_range in self.action_ranges:
        if action_range.Contains(bounds.Bounds.CreateFromEvent(event)):
          slices_in_actions.append(event)
          break
    return slices_in_actions

  def AppendThreadSlices(self, thread):
    self.all_slices.extend(self.SlicesInActions(thread.all_slices))
    self.toplevel_slices.extend(self.SlicesInActions(thread.toplevel_slices))

  def AddResults(self, num_frames, results):
    cpu_per_frame = (float(self.cpu_time) / num_frames) if num_frames else 0
    results.Add(ThreadCpuTimeResultName(self.name), 'ms', cpu_per_frame)

  def AddDetailedResults(self, num_frames, results):
    slices_by_category = collections.defaultdict(list)
    for s in self.all_slices:
      slices_by_category[s.category].append(s)
    all_self_times = []
    for category, slices_in_category in slices_by_category.iteritems():
      self_time = sum([x.self_time for x in slices_in_category])
      all_self_times.append(self_time)
      self_time_result = (float(self_time) / num_frames) if num_frames else 0
      results.Add(ThreadDetailResultName(self.name, category),
                  'ms', self_time_result)
    all_measured_time = sum(all_self_times)
    all_action_time = sum([action.bounds for action in self.action_ranges])
    idle_time = max(0, all_action_time - all_measured_time)
    idle_time_result = (float(idle_time) / num_frames) if num_frames else 0
    results.Add(ThreadDetailResultName(self.name, "idle"),
                'ms', idle_time_result)


class ThreadTimesTimelineMetric(TimelineMetric):
  def __init__(self):
    super(ThreadTimesTimelineMetric, self).__init__(TRACING_MODE)
    # Minimal traces, for minimum noise in CPU-time measurements.
    self.trace_categories = MINIMAL_TRACE_CATEGORIES
    self.results_to_report = AllThreads
    self.details_to_report = NoThreads

  def Start(self, page, tab):
    # We need the other traces in order to have any details to report.
    if not self.details_to_report == NoThreads:
      self.trace_categories = DEFAULT_TRACE_CATEGORIES
    super(ThreadTimesTimelineMetric, self).Start(page, tab)

  def CountSlices(self, slices, substring):
    count = 0
    for event in slices:
      if substring in event.name:
        count += 1
    return count

  def AddResults(self, tab, results):
    # We need at least one action or we won't count any slices.
    assert len(self._action_ranges) > 0

    # Set up each thread category for consistant results.
    thread_category_results = {}
    for name in TimelineThreadCategories.values():
      thread_category_results[name] = ResultsForThread(self._model,
                                                       self._action_ranges,
                                                       name)

    # Group the slices by their thread category.
    for thread in self._model.GetAllThreads():
      thread_category = ThreadCategoryName(thread.name)
      thread_category_results[thread_category].AppendThreadSlices(thread)

    # Group all threads.
    for thread in self._model.GetAllThreads():
      thread_category_results['total_all'].AppendThreadSlices(thread)

    # Also group fast-path threads.
    for thread in self._model.GetAllThreads():
      if ThreadCategoryName(thread.name) in FastPathThreads:
        thread_category_results['total_fast_path'].AppendThreadSlices(thread)

    # Calculate the number of frames.
    frame_slices = thread_category_results[FrameTraceThreadName].all_slices
    num_frames = self.CountSlices(frame_slices, FrameTraceName)

    # Report the desired results and details.
    for thread_results in thread_category_results.values():
      if thread_results.name in self.results_to_report:
        thread_results.AddResults(num_frames, results)
      # TOOD(nduca): When generic results objects are done, this special case
      # can be replaced with a generic UI feature.
      if thread_results.name in self.details_to_report:
        thread_results.AddDetailedResults(num_frames, results)

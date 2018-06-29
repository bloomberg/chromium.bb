# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

from telemetry.util.statistics import DivideIfPossibleOrZero
from telemetry.value import scalar
from telemetry.web_perf.metrics import timeline_based_metric


# We want to generate a consistent picture of our thread usage, despite
# having several process configurations (in-proc-gpu/single-proc).
# Since we can't isolate renderer threads in single-process mode, we
# always sum renderer-process threads' times. We also sum all io-threads
# for simplicity.
TimelineThreadCategories = {
    "Chrome_InProcGpuThread": "GPU",
    "CrGpuMain": "GPU",
    "VizCompositorThread": "display_compositor",
    "CrBrowserMain": "browser",
    "Browser Compositor": "browser",
    "CrRendererMain": "renderer_main",
    "Compositor": "renderer_compositor",
    "IOThread": "IO",
    "CompositorTileWorker": "raster",
    "DummyThreadName1": "other",
    "DummyThreadName2": "total_fast_path",
    "DummyThreadName3": "total_all"
}

_MatchBySubString = ["IOThread", "CompositorTileWorker"]

AllThreads = TimelineThreadCategories.values()
NoThreads = []
FastPathThreads = [
  "GPU", "display_compositor", "renderer_compositor", "browser", "IO"]

ReportMainThreadOnly = ["renderer_main"]
ReportSilkDetails = ["renderer_main"]

# TODO(epenner): Thread names above are likely fairly stable but trace names
# could change. We should formalize these traces to keep this robust.
OverheadTraceCategory = "trace_event_overhead"
OverheadTraceName = "overhead"
FrameTraceName = "Graphics.Pipeline"
FrameTraceStepName = "GenerateCompositorFrame"
FrameTraceThreadName = "renderer_compositor"

IntervalNames = ["frame", "second"]


def Rate(numerator, denominator):
  return DivideIfPossibleOrZero(numerator, denominator)


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


def ThreadCpuTimeResultName(thread_category, interval_name):
  # This isn't a good name, but I don't want to change it and lose continuity.
  return "thread_" + thread_category + "_cpu_time_per_" + interval_name


def ThreadTasksResultName(thread_category, interval_name):
  return "tasks_per_" + interval_name + "_" + thread_category


def ThreadMeanFrameTimeResultName(thread_category):
  return "mean_frame_time_" + thread_category


def ThreadDetailResultName(thread_category, interval_name, detail):
  detail_sanitized = detail.replace(".", "_")
  interval_sanitized = ""
  # Special-case per-frame detail names to preserve continuity.
  if interval_name == "frame":
    interval_sanitized = ""
  else:
    interval_sanitized = "_per_" + interval_name
  return (
      "thread_" + thread_category + interval_sanitized + "|" + detail_sanitized)


def ThreadCpuTimeUnits(interval_name):
  if interval_name == "second":
    return "%"
  return "ms"


def ThreadCpuTimeValue(ms_cpu_time_per_interval, interval_name):
  # When measuring seconds of CPU time per second of system time, report a %.
  if interval_name == "second":
    return (ms_cpu_time_per_interval / 1000.0) * 100.0
  return ms_cpu_time_per_interval


class ResultsForThread(object):

  def __init__(self, model, record_ranges, name):
    self.model = model
    self.toplevel_slices = []
    self.all_slices = []
    self.name = name
    self.record_ranges = record_ranges
    self.all_action_time = \
        sum([record_range.bounds for record_range in self.record_ranges])

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
      if x.thread_duration is None:
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
      for record_range in self.record_ranges:
        if record_range.ContainsInterval(event.start, event.end):
          slices_in_actions.append(event)
          break
    return slices_in_actions

  def AppendThreadSlices(self, thread):
    self.all_slices.extend(self.SlicesInActions(thread.all_slices))
    self.toplevel_slices.extend(self.SlicesInActions(thread.toplevel_slices))

  # Reports cpu-time per interval and tasks per interval.
  def AddResults(self, num_intervals, interval_name, results):
    cpu_per_interval = Rate(self.cpu_time, num_intervals)
    tasks_per_interval = Rate(len(self.toplevel_slices), num_intervals)
    results.AddValue(scalar.ScalarValue(
        results.current_page,
        ThreadCpuTimeResultName(self.name, interval_name),
        ThreadCpuTimeUnits(interval_name),
        ThreadCpuTimeValue(cpu_per_interval, interval_name)))
    results.AddValue(scalar.ScalarValue(
        results.current_page,
        ThreadTasksResultName(self.name, interval_name),
        "tasks", tasks_per_interval))

  def AddDetailedResults(self, num_intervals, interval_name, results):
    slices_by_category = collections.defaultdict(list)
    for s in self.all_slices:
      slices_by_category[s.category].append(s)
    all_self_times = []
    for category, slices_in_category in slices_by_category.iteritems():
      self_time = sum([x.self_time for x in slices_in_category])
      all_self_times.append(self_time)
      self_time_per_interval = Rate(self_time, num_intervals)
      results.AddValue(scalar.ScalarValue(
          results.current_page,
          ThreadDetailResultName(self.name, interval_name, category),
          ThreadCpuTimeUnits(interval_name),
          ThreadCpuTimeValue(self_time_per_interval, interval_name)))
    all_measured_time = sum(all_self_times)
    idle_time = max(0, self.all_action_time - all_measured_time)
    idle_time_per_interval = Rate(idle_time, num_intervals)
    results.AddValue(scalar.ScalarValue(
        results.current_page,
        ThreadDetailResultName(self.name, interval_name, "idle"),
        ThreadCpuTimeUnits(interval_name),
        ThreadCpuTimeValue(idle_time_per_interval, interval_name)))

  def CountTracesWithNameAndArg(self, trace_name, step):
    count = 0
    for event in self.all_slices:
      if trace_name in event.name and event.args and event.args["step"] == step:
        count += 1
    return count


class ThreadTimesTimelineMetric(timeline_based_metric.TimelineBasedMetric):

  def __init__(self):
    super(ThreadTimesTimelineMetric, self).__init__()
    # Minimal traces, for minimum noise in CPU-time measurements.
    self.results_to_report = AllThreads
    self.details_to_report = NoThreads

  def AddResults(self, model, _, interaction_records, results):
    # Set up each thread category for consistent results.
    thread_category_results = {}
    for name in TimelineThreadCategories.values():
      thread_category_results[name] = ResultsForThread(
          model, [r.GetBounds() for r in interaction_records], name)

    # Group the slices by their thread category.
    for thread in model.GetAllThreads():
      thread_category = ThreadCategoryName(thread.name)
      thread_category_results[thread_category].AppendThreadSlices(thread)

    # Group all threads.
    for thread in model.GetAllThreads():
      thread_category_results["total_all"].AppendThreadSlices(thread)

    # Also group fast-path threads.
    for thread in model.GetAllThreads():
      if ThreadCategoryName(thread.name) in FastPathThreads:
        thread_category_results["total_fast_path"].AppendThreadSlices(thread)

    # Calculate the interaction's number of frames.
    frame_rate_thread = thread_category_results[FrameTraceThreadName]
    num_frames = frame_rate_thread.CountTracesWithNameAndArg(FrameTraceName,
        FrameTraceStepName)

    # Calculate the interaction's duration.
    all_threads = thread_category_results["total_all"]
    num_seconds = all_threads.all_action_time / 1000.0

    # Report the desired results and details for each interval type.
    intervals = [("frame", num_frames), ("second", num_seconds)]
    for (interval_name, num_intervals) in intervals:
      for thread_results in thread_category_results.values():
        if thread_results.name in self.results_to_report:
          thread_results.AddResults(num_intervals, interval_name, results)
        # TODO(nduca): When generic results objects are done, this special case
        # can be replaced with a generic UI feature.
        if thread_results.name in self.details_to_report:
          thread_results.AddDetailedResults(
              num_intervals, interval_name, results)

    # Report mean frame time for the frame rate thread. We could report other
    # frame rates (e.g. renderer_main) but this might get confusing.
    mean_frame_time = Rate(frame_rate_thread.all_action_time, num_frames)
    results.AddValue(scalar.ScalarValue(
        results.current_page,
        ThreadMeanFrameTimeResultName(FrameTraceThreadName),
        "ms", mean_frame_time))

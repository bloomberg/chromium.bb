# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import collections

from core import path_util
from core import perf_benchmark

from page_sets import webgl_supported_shared_state

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry.page import shared_page_state
from telemetry import story
from telemetry.timeline import bounds
from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

from telemetry.value import list_of_scalar_values
from telemetry.value import trace


BLINK_PERF_BASE_DIR = os.path.join(path_util.GetChromiumSrcDir(),
                                   'third_party', 'WebKit', 'PerformanceTests')
SKIPPED_FILE = os.path.join(BLINK_PERF_BASE_DIR, 'Skipped')

EventBoundary = collections.namedtuple('EventBoundary',
                                       ['type', 'wall_time', 'thread_time'])

MergedEvent = collections.namedtuple('MergedEvent',
                                     ['bounds', 'thread_or_wall_duration'])

def CreateStorySetFromPath(path, skipped_file,
                           shared_page_state_class=(
                               shared_page_state.SharedPageState)):
  assert os.path.exists(path)

  page_urls = []
  serving_dirs = set()

  def _AddPage(path):
    if not path.endswith('.html'):
      return
    if '../' in open(path, 'r').read():
      # If the page looks like it references its parent dir, include it.
      serving_dirs.add(os.path.dirname(os.path.dirname(path)))
    page_urls.append('file://' + path.replace('\\', '/'))

  def _AddDir(dir_path, skipped):
    for candidate_path in os.listdir(dir_path):
      if candidate_path == 'resources':
        continue
      candidate_path = os.path.join(dir_path, candidate_path)
      if candidate_path.startswith(skipped):
        continue
      if os.path.isdir(candidate_path):
        _AddDir(candidate_path, skipped)
      else:
        _AddPage(candidate_path)

  if os.path.isdir(path):
    skipped = []
    if os.path.exists(skipped_file):
      for line in open(skipped_file, 'r').readlines():
        line = line.strip()
        if line and not line.startswith('#'):
          skipped_path = os.path.join(os.path.dirname(skipped_file), line)
          skipped.append(skipped_path.replace('/', os.sep))
    _AddDir(path, tuple(skipped))
  else:
    _AddPage(path)
  ps = story.StorySet(base_dir=os.getcwd() + os.sep,
                      serving_dirs=serving_dirs)

  all_urls = [p.rstrip('/') for p in page_urls]
  common_prefix = os.path.dirname(os.path.commonprefix(all_urls))
  for url in page_urls:
    name = url[len(common_prefix):].strip('/')
    ps.AddStory(page_module.Page(
        url, ps, ps.base_dir,
        shared_page_state_class=shared_page_state_class,
        name=name))
  return ps

def _CreateMergedEventsBoundaries(events, max_start_time):
  """ Merge events with the given |event_name| and return a list of MergedEvent
  objects. All events that are overlapping are megred together. Same as a union
  operation.

  Note: When merging multiple events, we approximate the thread_duration:
  duration = (last.thread_start + last.thread_duration) - first.thread_start

  Args:
    events: a list of TimelineEvents
    max_start_time: the maximum time that a TimelineEvent's start value can be.
    Events past this this time will be ignored.

  Returns:
    a sorted list of MergedEvent objects which contain a Bounds object of the
    wall time boundary and a thread_or_wall_duration which contains the thread
    duration if possible, and otherwise the wall duration.
  """
  event_boundaries = []  # Contains EventBoundary objects.
  merged_event_boundaries = []     # Contains MergedEvent objects.

  # Deconstruct our trace events into boundaries, sort, and then create
  # MergedEvents.
  # This is O(N*log(N)), although this can be done in O(N) with fancy
  # datastructures.
  # Note: When merging multiple events, we approximate the thread_duration:
  # dur = (last.thread_start + last.thread_duration) - first.thread_start
  for event in events:
    # Handle events where thread duration is None (async events).
    thread_start = None
    thread_end = None
    if event.thread_start and event.thread_duration:
      thread_start = event.thread_start
      thread_end = event.thread_start + event.thread_duration
    event_boundaries.append(
        EventBoundary("start", event.start, thread_start))
    event_boundaries.append(
        EventBoundary("end", event.end, thread_end))
  event_boundaries.sort(key=lambda e: e[1])

  # Merge all trace events that overlap.
  event_counter = 0
  curr_bounds = None
  curr_thread_start = None
  for event_boundary in event_boundaries:
    if event_boundary.type == "start":
      event_counter += 1
    else:
      event_counter -= 1
    # Initialization
    if curr_bounds is None:
      assert event_boundary.type == "start"
      # Exit early if we reach the max time.
      if event_boundary.wall_time > max_start_time:
        return merged_event_boundaries
      curr_bounds = bounds.Bounds()
      curr_bounds.AddValue(event_boundary.wall_time)
      curr_thread_start = event_boundary.thread_time
      continue
    # Create a the final bounds and thread duration when our event grouping
    # is over.
    if event_counter == 0:
      curr_bounds.AddValue(event_boundary.wall_time)
      thread_or_wall_duration = curr_bounds.bounds
      if curr_thread_start and event_boundary.thread_time:
        thread_or_wall_duration = event_boundary.thread_time - curr_thread_start
      merged_event_boundaries.append(
          MergedEvent(curr_bounds, thread_or_wall_duration))
      curr_bounds = None
  return merged_event_boundaries

def _ComputeTraceEventsThreadTimeForBlinkPerf(
    model, renderer_thread, trace_events_to_measure):
  """ Compute the CPU duration for each of |trace_events_to_measure| during
  blink_perf test.

  Args:
    renderer_thread: the renderer thread which run blink_perf test.
    trace_events_to_measure: a list of string names of trace events to measure
    CPU duration for.

  Returns:
    a dictionary in which each key is a trace event' name (from
    |trace_events_to_measure| list), and value is a list of numbers that
    represents to total cpu time of that trace events in each blink_perf test.
  """
  trace_cpu_time_metrics = {}

  # Collect the bounds of "blink_perf.runTest" events.
  test_runs_bounds = []
  for event in renderer_thread.async_slices:
    if event.name == "blink_perf.runTest":
      test_runs_bounds.append(bounds.Bounds.CreateFromEvent(event))
  test_runs_bounds.sort(key=lambda b: b.min)

  for t in trace_events_to_measure:
    trace_cpu_time_metrics[t] = [0.0] * len(test_runs_bounds)

  # Handle case where there are no tests.
  if not test_runs_bounds:
    return trace_cpu_time_metrics

  for event_name in trace_events_to_measure:
    merged_event_boundaries = _CreateMergedEventsBoundaries(
        model.IterAllEventsOfName(event_name), test_runs_bounds[-1].max)

    curr_test_runs_bound_index = 0
    for b in merged_event_boundaries:
      if b.bounds.bounds == 0:
        continue
      # Fast forward (if needed) to the first relevant test.
      while (curr_test_runs_bound_index < len(test_runs_bounds) and
             b.bounds.min > test_runs_bounds[curr_test_runs_bound_index].max):
        curr_test_runs_bound_index += 1
      if curr_test_runs_bound_index >= len(test_runs_bounds):
        break
      # Add metrics for all intersecting tests, as there may be multiple
      # tests that intersect with the event bounds.
      start_index = curr_test_runs_bound_index
      while (curr_test_runs_bound_index < len(test_runs_bounds) and
             b.bounds.Intersects(
                 test_runs_bounds[curr_test_runs_bound_index])):
        intersect_wall_time = bounds.Bounds.GetOverlapBetweenBounds(
            test_runs_bounds[curr_test_runs_bound_index], b.bounds)
        intersect_cpu_or_wall_time = (
            intersect_wall_time * b.thread_or_wall_duration / b.bounds.bounds)
        trace_cpu_time_metrics[event_name][curr_test_runs_bound_index] += (
            intersect_cpu_or_wall_time)
        curr_test_runs_bound_index += 1
      # Rewind to the last intersecting test as it might intersect with the
      # next event.
      curr_test_runs_bound_index = max(start_index,
                                       curr_test_runs_bound_index - 1)
  return trace_cpu_time_metrics


class _BlinkPerfMeasurement(legacy_page_test.LegacyPageTest):
  """Tuns a blink performance test and reports the results."""

  def __init__(self):
    super(_BlinkPerfMeasurement, self).__init__()
    with open(os.path.join(os.path.dirname(__file__),
                           'blink_perf.js'), 'r') as f:
      self._blink_perf_js = f.read()
    self._extra_chrome_categories = None

  def WillNavigateToPage(self, page, tab):
    del tab  # unused
    page.script_to_evaluate_on_commit = self._blink_perf_js

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--js-flags=--expose_gc',
        '--enable-experimental-web-platform-features',
        # Note that both this flag:
        '--ignore-autoplay-restrictions',
        # and this flag:
        '--disable-gesture-requirement-for-media-playback',
        # should be used until every build from
        # ToT to Stable switches over to one flag or another. This is to support
        # reference builds.
        # --disable-gesture-requirement-for-media-playback is the old one and
        # can be removed after M60 goes to stable.
        '--enable-experimental-canvas-features'
    ])

  def SetOptions(self, options):
    super(_BlinkPerfMeasurement, self).SetOptions(options)
    browser_type = options.browser_options.browser_type
    if browser_type and 'content-shell' in browser_type:
      options.AppendExtraBrowserArgs('--expose-internals-for-testing')
    if options.extra_chrome_categories:
      self._extra_chrome_categories = options.extra_chrome_categories

  def _ContinueTestRunWithTracing(self, tab):
    tracing_categories = tab.EvaluateJavaScript(
        'testRunner.tracingCategories')
    config = tracing_config.TracingConfig()
    config.enable_chrome_trace = True
    config.chrome_trace_config.category_filter.AddFilterString(
        'blink.console')  # This is always required for js land trace event
    config.chrome_trace_config.category_filter.AddFilterString(
        tracing_categories)
    if self._extra_chrome_categories:
      config.chrome_trace_config.category_filter.AddFilterString(
          self._extra_chrome_categories)
    tab.browser.platform.tracing_controller.StartTracing(config)
    tab.EvaluateJavaScript('testRunner.scheduleTestRun()')
    tab.WaitForJavaScriptCondition('testRunner.isDone')

    trace_data = tab.browser.platform.tracing_controller.StopTracing()

    # TODO(charliea): This is part of a three-sided Chromium/Telemetry patch
    # where we're changing the return type of StopTracing from a TraceValue to a
    # (TraceValue, nonfatal_exception_list) tuple. Once the tuple return value
    # lands in Chromium, the non-tuple logic should be deleted.
    if isinstance(trace_data, tuple):
      trace_data = trace_data[0]

    return trace_data


  def PrintAndCollectTraceEventMetrics(self, trace_cpu_time_metrics, results):
    unit = 'ms'
    print
    for trace_event_name, cpu_times in trace_cpu_time_metrics.iteritems():
      print 'CPU times of trace event "%s":' % trace_event_name
      cpu_times_string = ', '.join(['{0:.10f}'.format(t) for t in cpu_times])
      print 'values %s %s' % (cpu_times_string, unit)
      avg = 0.0
      if cpu_times:
        avg = sum(cpu_times)/len(cpu_times)
      print 'avg', '{0:.10f}'.format(avg), unit
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, name=trace_event_name, units=unit,
          values=cpu_times))
      print
    print '\n'

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptCondition(
        'testRunner.isDone || testRunner.isWaitingForTracingStart', timeout=600)
    trace_cpu_time_metrics = {}
    if tab.EvaluateJavaScript('testRunner.isWaitingForTracingStart'):
      trace_data = self._ContinueTestRunWithTracing(tab)
      # TODO(#763375): Rely on results.telemetry_info.trace_local_path/etc.
      kwargs = {}
      if hasattr(results.telemetry_info, 'trace_local_path'):
        kwargs['file_path'] = results.telemetry_info.trace_local_path
        kwargs['remote_path'] = results.telemetry_info.trace_remote_path
        kwargs['upload_bucket'] = results.telemetry_info.upload_bucket
        kwargs['cloud_url'] = results.telemetry_info.trace_remote_url
      trace_value = trace.TraceValue(page, trace_data, **kwargs)
      results.AddValue(trace_value)

      trace_events_to_measure = tab.EvaluateJavaScript(
          'window.testRunner.traceEventsToMeasure')
      model = model_module.TimelineModel(trace_data)
      renderer_thread = model.GetRendererThreadFromTabId(tab.id)
      trace_cpu_time_metrics = _ComputeTraceEventsThreadTimeForBlinkPerf(
          model, renderer_thread, trace_events_to_measure)

    log = tab.EvaluateJavaScript('document.getElementById("log").innerHTML')

    for line in log.splitlines():
      if line.startswith("FATAL: "):
        print line
        continue
      if not line.startswith('values '):
        continue
      parts = line.split()
      values = [float(v.replace(',', '')) for v in parts[1:-1]]
      units = parts[-1]
      metric = page.name.split('.')[0].replace('/', '_')
      if values:
        results.AddValue(list_of_scalar_values.ListOfScalarValues(
            results.current_page, metric, units, values))
      else:
        raise legacy_page_test.MeasurementFailure('Empty test results')

      break

    print log

    self.PrintAndCollectTraceEventMetrics(trace_cpu_time_metrics, results)


class _BlinkPerfBenchmark(perf_benchmark.PerfBenchmark):

  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.' + cls.tag

  def CreateStorySet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, self.subdir)
    return CreateStorySetFromPath(path, SKIPPED_FILE)


@benchmark.Owner(emails=['jbroman@chromium.org',
                         'yukishiino@chromium.org',
                         'haraken@chromium.org'])
class BlinkPerfBindings(_BlinkPerfBenchmark):
  tag = 'bindings'
  subdir = 'Bindings'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory(
            'structured-clone-long-string-serialize.html',
            [story.expectations.ANDROID_ONE],
            'crbug.com/764868')
        self.DisableStory(
            'structured-clone-json-serialize.html',
            [story.expectations.ANDROID_ONE],
            'crbug.com/764868')
        self.DisableStory(
            'structured-clone-long-string-deserialize.html',
            [story.expectations.ANDROID_ONE],
            'crbug.com/764868')
        self.DisableStory(
            'structured-clone-json-deserialize.html',
            [story.expectations.ANDROID_ONE],
            'crbug.com/764868')
    return StoryExpectations()


@benchmark.Owner(emails=['rune@opera.com'])
class BlinkPerfCSS(_BlinkPerfBenchmark):
  tag = 'css'
  subdir = 'CSS'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['junov@chromium.org'])
class BlinkPerfCanvas(_BlinkPerfBenchmark):
  tag = 'canvas'
  subdir = 'Canvas'

  def CreateStorySet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, self.subdir)
    story_set = CreateStorySetFromPath(
        path, SKIPPED_FILE,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState))
    # WebGLSupportedSharedState requires the skipped_gpus property to
    # be set on each page.
    for page in story_set:
      page.skipped_gpus = []
    return story_set

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story.expectations.ANDROID_SVELTE],
                              'crbug.com/593973')
        self.DisableStory('putImageData.html',
            [story.expectations.ANDROID_NEXUS6], 'crbug.com/738453')
        # pylint: disable=line-too-long
        self.DisableStory('draw-static-canvas-2d-to-hw-accelerated-canvas-2d.html',
            [story.expectations.ANDROID_NEXUS6], 'crbug.com/765799')
        self.DisableStory(
            'draw-static-canvas-2d-to-hw-accelerated-canvas-2d.html',
            [story.expectations.ANDROID_NEXUS5,
             story.expectations.ANDROID_NEXUS5X],
            'crbug.com/784540')
        self.DisableStory(
            'draw-dynamic-canvas-2d-to-hw-accelerated-canvas-2d.html',
            [story.expectations.ANDROID_NEXUS5,
             story.expectations.ANDROID_NEXUS5X],
            'crbug.com/784540')
    return StoryExpectations()

@benchmark.Owner(emails=['jbroman@chromium.org',
                         'yukishiino@chromium.org',
                         'haraken@chromium.org'])
class BlinkPerfDOM(_BlinkPerfBenchmark):
  tag = 'dom'
  subdir = 'DOM'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['hayato@chromium.org'])
class BlinkPerfEvents(_BlinkPerfBenchmark):
  tag = 'events'
  subdir = 'Events'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['cblume@chromium.org'])
class BlinkPerfImageDecoder(_BlinkPerfBenchmark):
  tag = 'image_decoder'
  subdir = 'ImageDecoder'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-blink-features=JSImageDecode',
    ])

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['eae@chromium.org'])
class BlinkPerfLayout(_BlinkPerfBenchmark):
  tag = 'layout'
  subdir = 'Layout'

  def GetExpectations(self):
    class Expectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story.expectations.ANDROID_SVELTE],
                              'crbug.com/551950')
    return Expectations()


@benchmark.Owner(emails=['dmurph@chromium.org'])
class BlinkPerfOWPStorage(_BlinkPerfBenchmark):
  tag = 'owp_storage'
  subdir = 'OWPStorage'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['wangxianzhu@chromium.org'])
class BlinkPerfPaint(_BlinkPerfBenchmark):
  tag = 'paint'
  subdir = 'Paint'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story.expectations.ANDROID_SVELTE],
                              'crbug.com/574483')
    return StoryExpectations()


@benchmark.Owner(emails=['jbroman@chromium.org',
                         'yukishiino@chromium.org',
                         'haraken@chromium.org'])
class BlinkPerfParser(_BlinkPerfBenchmark):
  tag = 'parser'
  subdir = 'Parser'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['kouhei@chromium.org', 'fs@opera.com'])
class BlinkPerfSVG(_BlinkPerfBenchmark):
  tag = 'svg'
  subdir = 'SVG'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('Debian.html', [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('FlowerFromMyGarden.html',
                          [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('HarveyRayner.html',
                          [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('SvgCubics.html',
                          [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('SvgNestedUse.html',
                          [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('Worldcup.html', [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
        self.DisableStory('CrawFishGanson.html',
                          [story.expectations.ANDROID_NEXUS5X],
                          'crbug.com/736817')
    return StoryExpectations()


@benchmark.Owner(emails=['hayato@chromium.org'])
class BlinkPerfShadowDOM(_BlinkPerfBenchmark):
  tag = 'shadow_dom'
  subdir = 'ShadowDOM'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story.expectations.ANDROID_NEXUS5X],
                              'crbug.com/702319')
    return StoryExpectations()

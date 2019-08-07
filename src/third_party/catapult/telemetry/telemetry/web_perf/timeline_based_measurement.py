# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import logging

from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config
from telemetry.value import trace
from telemetry.web_perf.metrics import timeline_based_metric
from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_interaction_record as tir_module

# TimelineBasedMeasurement considers all instrumentation as producing a single
# timeline. But, depending on the amount of instrumentation that is enabled,
# overhead increases. The user of the measurement must therefore chose between
# a few levels of instrumentation.
LOW_OVERHEAD_LEVEL = 'low-overhead'
DEFAULT_OVERHEAD_LEVEL = 'default-overhead'
DEBUG_OVERHEAD_LEVEL = 'debug-overhead'

ALL_OVERHEAD_LEVELS = [
    LOW_OVERHEAD_LEVEL,
    DEFAULT_OVERHEAD_LEVEL,
    DEBUG_OVERHEAD_LEVEL,
]


class InvalidInteractions(Exception):
  pass


# TODO(nednguyen): Get rid of this results wrapper hack after we add interaction
# record to telemetry value system (crbug.com/453109)
class ResultsWrapperInterface(object):
  def __init__(self):
    self._tir_label = None
    self._results = None

  def SetResults(self, results):
    self._results = results

  def SetTirLabel(self, tir_label):
    self._tir_label = tir_label

  @property
  def current_page(self):
    return self._results.current_page

  def AddValue(self, value):
    raise NotImplementedError


class _TBMResultWrapper(ResultsWrapperInterface):
  def AddValue(self, value):
    assert self._tir_label
    if value.tir_label:
      assert value.tir_label == self._tir_label
    else:
      value.tir_label = self._tir_label
    self._results.AddValue(value)


def _GetRendererThreadsToInteractionRecordsMap(model):
  threads_to_records_map = collections.defaultdict(list)
  interaction_labels_of_previous_threads = set()
  for curr_thread in model.GetAllThreads():
    for event in curr_thread.async_slices:
      # TODO(nduca): Add support for page-load interaction record.
      if tir_module.IsTimelineInteractionRecord(event.name):
        interaction = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
        threads_to_records_map[curr_thread].append(interaction)
        if interaction.label in interaction_labels_of_previous_threads:
          raise InvalidInteractions(
              'Interaction record label %s is duplicated on different '
              'threads' % interaction.label)
    if curr_thread in threads_to_records_map:
      interaction_labels_of_previous_threads.update(
          r.label for r in threads_to_records_map[curr_thread])

  return threads_to_records_map


class _TimelineBasedMetrics(object):
  def __init__(self, model, renderer_thread, interaction_records,
               results_wrapper, metrics):
    self._model = model
    self._renderer_thread = renderer_thread
    self._interaction_records = interaction_records
    self._results_wrapper = results_wrapper
    self._all_metrics = metrics

  def AddResults(self, results):
    interactions_by_label = collections.defaultdict(list)
    for i in self._interaction_records:
      interactions_by_label[i.label].append(i)

    for label, interactions in interactions_by_label.iteritems():
      are_repeatable = [i.repeatable for i in interactions]
      if not all(are_repeatable) and len(interactions) > 1:
        raise InvalidInteractions('Duplicate unrepeatable interaction records '
                                  'on the page')
      self._results_wrapper.SetResults(results)
      self._results_wrapper.SetTirLabel(label)
      self.UpdateResultsByMetric(interactions, self._results_wrapper)

  def UpdateResultsByMetric(self, interactions, wrapped_results):
    if not interactions:
      return

    for metric in self._all_metrics:
      metric.AddResults(self._model, self._renderer_thread,
                        interactions, wrapped_results)


class Options(object):
  """A class to be used to configure TimelineBasedMeasurement.

  This is created and returned by
  Benchmark.CreateCoreTimelineBasedMeasurementOptions.

  """

  def __init__(self, overhead_level=LOW_OVERHEAD_LEVEL):
    """As the amount of instrumentation increases, so does the overhead.
    The user of the measurement chooses the overhead level that is appropriate,
    and the tracing is filtered accordingly.

    overhead_level: Can either be a custom ChromeTraceCategoryFilter object or
        one of LOW_OVERHEAD_LEVEL, DEFAULT_OVERHEAD_LEVEL or
        DEBUG_OVERHEAD_LEVEL.
    """
    self._config = tracing_config.TracingConfig()
    self._config.enable_chrome_trace = True
    self._config.enable_platform_display_trace = False

    if isinstance(overhead_level,
                  chrome_trace_category_filter.ChromeTraceCategoryFilter):
      self._config.chrome_trace_config.SetCategoryFilter(overhead_level)
    elif overhead_level in ALL_OVERHEAD_LEVELS:
      if overhead_level == LOW_OVERHEAD_LEVEL:
        self._config.chrome_trace_config.SetLowOverheadFilter()
      elif overhead_level == DEFAULT_OVERHEAD_LEVEL:
        self._config.chrome_trace_config.SetDefaultOverheadFilter()
      else:
        self._config.chrome_trace_config.SetDebugOverheadFilter()
    else:
      raise Exception("Overhead level must be a ChromeTraceCategoryFilter "
                      "object or valid overhead level string. Given overhead "
                      "level: %s" % overhead_level)

    self._timeline_based_metrics = None
    self._legacy_timeline_based_metrics = []


  def ExtendTraceCategoryFilter(self, filters):
    for category_filter in filters:
      self.AddTraceCategoryFilter(category_filter)

  def AddTraceCategoryFilter(self, category_filter):
    self._config.chrome_trace_config.category_filter.AddFilter(category_filter)

  @property
  def category_filter(self):
    return self._config.chrome_trace_config.category_filter

  @property
  def config(self):
    return self._config

  def ExtendTimelineBasedMetric(self, metrics):
    for metric in metrics:
      self.AddTimelineBasedMetric(metric)

  def AddTimelineBasedMetric(self, metric):
    assert isinstance(metric, basestring)
    if self._timeline_based_metrics is None:
      self._timeline_based_metrics = []
    self._timeline_based_metrics.append(metric)

  def SetTimelineBasedMetrics(self, metrics):
    """Sets the new-style (TBMv2) metrics to run.

    Metrics are assumed to live in //tracing/tracing/metrics, so the path you
    pass in should be relative to that. For example, to specify
    sample_metric.html, you should pass in ['sample_metric.html'].

    Args:
      metrics: A list of strings giving metric paths under
          //tracing/tracing/metrics.
    """
    assert isinstance(metrics, list)
    for metric in metrics:
      assert isinstance(metric, basestring)
    self._timeline_based_metrics = metrics

  def GetTimelineBasedMetrics(self):
    return self._timeline_based_metrics

  def SetLegacyTimelineBasedMetrics(self, metrics):
    assert isinstance(metrics, collections.Iterable)
    for m in metrics:
      assert isinstance(m, timeline_based_metric.TimelineBasedMetric)
    self._legacy_timeline_based_metrics = metrics

  def GetLegacyTimelineBasedMetrics(self):
    return self._legacy_timeline_based_metrics


class TimelineBasedMeasurement(story_test.StoryTest):
  """Collects multiple metrics based on their interaction records.

  A timeline based measurement shifts the burden of what metrics to collect onto
  the story under test. Instead of the measurement
  having a fixed set of values it collects, the story being tested
  issues (via javascript) an Interaction record into the user timing API that
  describing what is happening at that time, as well as a standardized set
  of flags describing the semantics of the work being done. The
  TimelineBasedMeasurement object collects a trace that includes both these
  interaction records, and a user-chosen amount of performance data using
  Telemetry's various timeline-producing APIs, tracing especially.

  It then passes the recorded timeline to different TimelineBasedMetrics based
  on those flags. As an example, this allows a single story run to produce
  load timing data, smoothness data, critical jank information and overall cpu
  usage information.

  For information on how to mark up a page to work with
  TimelineBasedMeasurement, refer to the
  perf.metrics.timeline_interaction_record module.

  Args:
      options: an instance of timeline_based_measurement.Options.
      results_wrapper: A class that has the __init__ method takes in
        the page_test_results object and the interaction record label. This
        class follows the ResultsWrapperInterface. Note: this class is not
        supported long term and to be removed when crbug.com/453109 is resolved.
  """
  def __init__(self, options, results_wrapper=None):
    self._tbm_options = options
    self._results_wrapper = results_wrapper or _TBMResultWrapper()

  def WillRunStory(self, platform):
    """Configure and start tracing."""
    if not platform.tracing_controller.IsChromeTracingSupported():
      raise Exception('Not supported')
    if self._tbm_options.config.enable_chrome_trace:
      # Always enable 'blink.console' and 'v8.console' categories for:
      # 1) Backward compat of chrome clock sync (https://crbug.com/646925).
      # 2) Allows users to add trace event through javascript.
      # 3) For the console error metric (https://crbug.com/880432).
      # Note that these categories are extremely low-overhead, so this doesn't
      # affect the tracing overhead budget much.
      chrome_config = self._tbm_options.config.chrome_trace_config
      chrome_config.category_filter.AddIncludedCategory('blink.console')
      chrome_config.category_filter.AddIncludedCategory('v8.console')
    platform.tracing_controller.StartTracing(self._tbm_options.config)

  def Measure(self, platform, results):
    """Collect all possible metrics and added them to results."""
    platform.tracing_controller.SetTelemetryInfo(results.telemetry_info)
    trace_result = platform.tracing_controller.StopTracing()
    trace_value = trace.TraceValue(
        results.current_page, trace_result,
        file_path=results.telemetry_info.trace_local_path,
        remote_path=results.telemetry_info.trace_remote_path,
        upload_bucket=results.telemetry_info.upload_bucket,
        cloud_url=results.telemetry_info.trace_remote_url,
        trace_url=results.telemetry_info.trace_url)
    results.AddValue(trace_value)
    if self._tbm_options.GetTimelineBasedMetrics():
      assert not self._tbm_options.GetLegacyTimelineBasedMetrics(), (
          'Specifying both TBMv1 and TBMv2 metrics is not allowed.')
      # The metrics computation happens later asynchronously in
      # results.ComputeTimelineBasedMetrics().
      trace_value.SetTimelineBasedMetrics(
          self._tbm_options.GetTimelineBasedMetrics())
    else:
      # Run all TBMv1 metrics if no other metric is specified
      # (legacy behavior)
      if not self._tbm_options.GetLegacyTimelineBasedMetrics():
        raise Exception(
            'Please specify the TBMv1 metrics you are interested in '
            'explicitly.')
      trace_value.SerializeTraceData()
      self._ComputeLegacyTimelineBasedMetrics(results, trace_result)

  def DidRunStory(self, platform, results):
    """Clean up after running the story."""
    if platform.tracing_controller.is_tracing_running:
      trace_result = platform.tracing_controller.StopTracing()
      trace_value = trace.TraceValue(
          results.current_page, trace_result,
          file_path=results.telemetry_info.trace_local_path,
          remote_path=results.telemetry_info.trace_remote_path,
          upload_bucket=results.telemetry_info.upload_bucket,
          cloud_url=results.telemetry_info.trace_remote_url)
      trace_value.SerializeTraceData()
      results.AddValue(trace_value)

  def _ComputeLegacyTimelineBasedMetrics(self, results, trace_result):
    model = model_module.TimelineModel(trace_result)
    threads_to_records_map = _GetRendererThreadsToInteractionRecordsMap(model)
    if (len(threads_to_records_map.values()) == 0 and
        self._tbm_options.config.enable_chrome_trace):
      logging.warning(
          'No timeline interaction records were recorded in the trace. '
          'This could be caused by console.time() & console.timeEnd() execution'
          ' failure or the tracing category specified doesn\'t include '
          'blink.console categories.')

    all_metrics = self._tbm_options.GetLegacyTimelineBasedMetrics()

    for renderer_thread, interaction_records in (
        threads_to_records_map.iteritems()):
      meta_metrics = _TimelineBasedMetrics(
          model, renderer_thread, interaction_records, self._results_wrapper,
          all_metrics)
      meta_metrics.AddResults(results)

    for metric in all_metrics:
      metric.AddWholeTraceResults(model, results)

  @property
  def tbm_options(self):
    return self._tbm_options

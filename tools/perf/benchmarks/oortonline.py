# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets

from core import perf_benchmark
from telemetry import benchmark
from telemetry.page import legacy_page_test
from telemetry.value import scalar
from telemetry.value import improvement_direction
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import v8_gc_latency
from telemetry.web_perf.metrics import smoothness
from telemetry.web_perf.metrics import memory_timeline


class _OortOnlineMeasurement(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(_OortOnlineMeasurement, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression('window.benchmarkFinished', 1000)
    scores = tab.EvaluateJavaScript('window.benchmarkScore')
    for score in scores:
      valid = score['valid']
      if valid:
        results.AddValue(scalar.ScalarValue(
            results.current_page, score['name'], 'score', score['score'],
            important=True, improvement_direction=improvement_direction.UP))


@benchmark.Disabled('android')
class OortOnline(perf_benchmark.PerfBenchmark):
  """OortOnline benchmark that measures WebGL and V8 performance.
  URL: http://oortonline.gl/#run
  Info: http://v8project.blogspot.de/2015/10/jank-busters-part-one.html
  """
  test = _OortOnlineMeasurement

  @classmethod
  def Name(cls):
    return 'oortonline'

  def CreateStorySet(self, options):
    return page_sets.OortOnlinePageSet()


# Disabled on reference builds because they don't support the new
# Tracing.requestMemoryDump DevTools API. See http://crbug.com/540022.
@benchmark.Disabled('reference')
@benchmark.Disabled('android')
class OortOnlineTBM(perf_benchmark.PerfBenchmark):
  """OortOnline benchmark that measures WebGL and V8 performance.
  URL: http://oortonline.gl/#run
  Info: http://v8project.blogspot.de/2015/10/jank-busters-part-one.html
  """

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        # TODO(perezju): Temporary workaround to disable periodic memory dumps.
        # See: http://crbug.com/513692
        '--enable-memory-benchmarking',
    ])

  def CreateStorySet(self, options):
    return page_sets.OortOnlineTBMPageSet()

  def CreateTimelineBasedMeasurementOptions(self):
    v8_gc_latency_categories = [
        'blink.console', 'renderer.scheduler', 'v8', 'webkit.console']
    smoothness_categories = [
        'webkit.console', 'blink.console', 'benchmark', 'trace_event_overhead']
    categories = list(set(v8_gc_latency_categories + smoothness_categories))
    memory_categories = 'blink.console,disabled-by-default-memory-infra'
    category_filter = tracing_category_filter.TracingCategoryFilter(
        memory_categories)
    for category in categories:
      category_filter.AddIncludedCategory(category)
    options = timeline_based_measurement.Options(category_filter)
    options.SetLegacyTimelineBasedMetrics([v8_gc_latency.V8GCLatency(),
                                     smoothness.SmoothnessMetric(),
                                     memory_timeline.MemoryTimelineMetric()])
    return options

  @classmethod
  def Name(cls):
    return 'oortonline_tbm'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    if value.tir_label in ['Begin', 'End']:
      return value.name.startswith('memory_') and 'v8_renderer' in value.name
    else:
      return value.tir_label == 'Running'

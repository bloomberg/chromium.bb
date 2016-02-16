# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import shlex

from core import perf_benchmark

from measurements import v8_detached_context_age_in_gc
from measurements import v8_gc_times
import page_sets
from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import v8_gc_latency
from telemetry.web_perf.metrics import v8_execution
from telemetry.web_perf.metrics import smoothness
from telemetry.web_perf.metrics import memory_timeline


@benchmark.Disabled('win')        # crbug.com/416502
class V8Top25(perf_benchmark.PerfBenchmark):
  """Measures V8 GC metrics on the while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.V8Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.top_25_smooth'


@benchmark.Enabled('android')
class V8KeyMobileSites(perf_benchmark.PerfBenchmark):
  """Measures V8 GC metrics on the while scrolling down key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.key_mobile_sites_smooth'


class V8DetachedContextAgeInGC(perf_benchmark.PerfBenchmark):
  """Measures the number of GCs needed to collect a detached context.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_detached_context_age_in_gc.V8DetachedContextAgeInGC
  page_set = page_sets.PageReloadCasesPageSet

  @classmethod
  def Name(cls):
    return 'v8.detached_context_age_in_gc'


# Disabled on reference builds because they don't support the new
# Tracing.requestMemoryDump DevTools API. See http://crbug.com/540022.
@benchmark.Disabled('reference', 'android')  # crbug.com/579546
class V8InfiniteScroll(perf_benchmark.PerfBenchmark):
  """Measures V8 GC metrics and memory usage while scrolling the top web pages.
  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""

  page_set = page_sets.InfiniteScrollPageSet

  def SetExtraBrowserOptions(self, options):
    existing_js_flags = []
    for extra_arg in options.extra_browser_args:
      if extra_arg.startswith('--js-flags='):
        existing_js_flags.extend(shlex.split(extra_arg[len('--js-flags='):]))
    options.AppendExtraBrowserArgs([
        # TODO(perezju): Temporary workaround to disable periodic memory dumps.
        # See: http://crbug.com/513692
        '--enable-memory-benchmarking',
        # This overrides any existing --js-flags, hence we have to include the
        # previous flags as well.
        '--js-flags="--heap-growing-percent=10 %s"' %
          (' '.join(existing_js_flags))
    ])

  def CreateTimelineBasedMeasurementOptions(self):
    v8_categories = [
        'blink.console', 'renderer.scheduler', 'v8', 'webkit.console']
    smoothness_categories = [
        'webkit.console', 'blink.console', 'benchmark', 'trace_event_overhead']
    categories = list(set(v8_categories + smoothness_categories))
    memory_categories = 'blink.console,disabled-by-default-memory-infra'
    category_filter = tracing_category_filter.TracingCategoryFilter(
        memory_categories)
    for category in categories:
      category_filter.AddIncludedCategory(category)
    options = timeline_based_measurement.Options(category_filter)
    options.SetLegacyTimelineBasedMetrics([v8_gc_latency.V8GCLatency(),
                                     v8_execution.V8ExecutionMetric(),
                                     smoothness.SmoothnessMetric(),
                                     memory_timeline.MemoryTimelineMetric()])
    return options

  @classmethod
  def Name(cls):
    return 'v8.infinite_scroll'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    if value.tir_label in ['Load', 'Wait']:
      return (value.name.startswith('v8_') and not
              value.name.startswith('v8_gc'))
    if value.tir_label in ['Begin', 'End']:
      return (value.name.startswith('memory_') and
              'v8_renderer' in value.name) or \
             (value.name.startswith('v8_') and not
              value.name.startswith('v8_gc'))
    else:
      return value.tir_label == 'Scrolling'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark


from measurements import v8_detached_context_age_in_gc
import page_sets

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Owner(emails=['ulan@chromium.org'])
class V8DetachedContextAgeInGC(perf_benchmark.PerfBenchmark):
  """Measures the number of GCs needed to collect a detached context.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_detached_context_age_in_gc.V8DetachedContextAgeInGC
  page_set = page_sets.PageReloadCasesPageSet

  @classmethod
  def Name(cls):
    return 'v8.detached_context_age_in_gc'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('Docs  (1 open document tab)',
                          [story.expectations.ALL_WIN],
                          'crbug.com/')
    return StoryExpectations()


class _Top25RuntimeStats(perf_benchmark.PerfBenchmark):
  options = {'pageset_repeat': 3}

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
      '--enable-blink-features=BlinkRuntimeCallStats')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    # TODO(fmeawad): most of the cat_filter information is extracted from
    # page_cycler_v2 TimelineBasedMeasurementOptionsForLoadingMetric because
    # used by the loadingMetric because the runtimeStatsMetric uses the
    # interactive time calculated internally by the loadingMetric.
    # It is better to share the code so that we can keep them in sync.
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "navigation" and "blink.user_timing" are needed to capture core
    # navigation events.
    cat_filter.AddIncludedCategory('navigation')
    cat_filter.AddIncludedCategory('blink.user_timing')

    # "loading" is needed for first-meaningful-paint computation.
    cat_filter.AddIncludedCategory('loading')

    # "toplevel" category is used to capture TaskQueueManager events
    # necessary to compute time-to-interactive.
    cat_filter.AddIncludedCategory('toplevel')

    # V8 needed categories
    cat_filter.AddIncludedCategory('v8')
    cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

    tbm_options = timeline_based_measurement.Options(
        overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetrics(['runtimeStatsMetric'])
    return tbm_options


@benchmark.Owner(emails=['cbruni@chromium.org'])
class V8Top25RuntimeStats(_Top25RuntimeStats):
  """Runtime Stats benchmark for a 25 top V8 web pages.

  Designed to represent a mix between top websites and a set of pages that
  have unique V8 characteristics.
  """

  @classmethod
  def Name(cls):
    return 'v8.runtime_stats.top_25'

  def CreateStorySet(self, options):
    return page_sets.V8Top25StorySet()

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark(
            [story.expectations.ALL_ANDROID, story.expectations.ALL_WIN],
            'crbug.com/664318')
    return StoryExpectations()

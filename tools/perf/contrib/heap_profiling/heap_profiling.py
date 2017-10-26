# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import loading_metrics_category

from core import perf_benchmark

from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement

import page_sets


class _HeapProfilingBenchmark(perf_benchmark.PerfBenchmark):
  """Bechmark to measure heap profiling overhead."""
  BROWSER_OPTIONS = NotImplemented

  page_set = page_sets.DesktopHeapProfilingStorySet
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,toplevel,rail,disabled-by-default-memory-infra')
    options = timeline_based_measurement.Options(cat_filter)
    options.SetTimelineBasedMetrics([
        'cpuTimeMetric',
        'loadingMetric',
        'memoryMetric',
        'tracingMetric',
    ])
    loading_metrics_category.AugmentOptionsForLoadingMetrics(options)
    # Disable periodic dumps by setting default config.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  def SetExtraBrowserOptions(self, options):
    super(_HeapProfilingBenchmark, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs(self.BROWSER_OPTIONS)

  def GetExpectations(self):
    class DefaultExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass  # No stories disabled.
    return DefaultExpectations()


class PseudoHeapProfilingBenchmark(_HeapProfilingBenchmark):
  BROWSER_OPTIONS = ['--enable-heap-profiling']

  @classmethod
  def Name(cls):
    return 'heap_profiling.pseudo'


class NativeHeapProfilingBenchmark(_HeapProfilingBenchmark):
  BROWSER_OPTIONS = ['--enable-heap-profiling=native']

  @classmethod
  def Name(cls):
    return 'heap_profiling.native'


class DisabledHeapProfilingBenchmark(_HeapProfilingBenchmark):
  BROWSER_OPTIONS = []

  @classmethod
  def Name(cls):
    return 'heap_profiling.disabled'

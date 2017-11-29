# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement

from contrib.leak_detection import page_sets

class _LeakDetectionBase(perf_benchmark.PerfBenchmark):
  """ A base class for leak detection benchmarks. """

  def CreateCoreTimelineBasedMeasurementOptions(self):
    tbm_options = timeline_based_measurement.Options(
        chrome_trace_category_filter.ChromeTraceCategoryFilter(
            '-*,disabled-by-default-memory-infra'))
    # Setting an empty memory dump config disables periodic dumps.
    tbm_options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    # Set required tracing categories for leak detection
    tbm_options.AddTimelineBasedMetric('leakDetectionMetric')
    return tbm_options

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--js-flags=--expose-gc')


@benchmark.Owner(emails=['yuzus@chromium.org'])
class MemoryLeakDetectionBenchmark(_LeakDetectionBase):
  page_set = page_sets.LeakDetectionStorySet

  @classmethod
  def Name(cls):
    return 'memory.leak_detection'

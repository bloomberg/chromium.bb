# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from core import perf_benchmark

from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import memory_timeline
from telemetry.web_perf.metrics import v8_gc_latency

import page_sets


class _MemoryInfra(perf_benchmark.PerfBenchmark):
  """Base class for new-generation memory benchmarks based on memory-infra.

  This benchmark records data using memory-infra (https://goo.gl/8tGc6O), which
  is part of chrome tracing, and extracts it using timeline-based measurements.
  """

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        # TODO(perezju): Temporary workaround to disable periodic memory dumps.
        # See: http://crbug.com/513692
        '--enable-memory-benchmarking',
    ])

  def CreateTimelineBasedMeasurementOptions(self):
    # Enable only memory-infra, to get memory dumps, and blink.console, to get
    # the timeline markers used for mapping threads to tabs.
    trace_memory = tracing_category_filter.TracingCategoryFilter(
        filter_string='-*,blink.console,disabled-by-default-memory-infra')
    tbm_options = timeline_based_measurement.Options(
        overhead_level=trace_memory)
    tbm_options.config.enable_android_graphics_memtrack = True
    return tbm_options

  @classmethod
  def HasTraceRerunDebugOption(cls):
    return True

  def SetupBenchmarkDefaultTraceRerunOptions(self, tbm_options):
    tbm_options.SetLegacyTimelineBasedMetrics((
        memory_timeline.MemoryTimelineMetric(),
    ))


# TODO(bashi): Workaround for http://crbug.com/532075
# @benchmark.Enabled('android') shouldn't be needed.
@benchmark.Enabled('android')
class MemoryHealthQuick(_MemoryInfra):
  """Timeline based benchmark for the Memory Health Plan (1 iteration)."""
  page_set = page_sets.MemoryHealthStory

  @classmethod
  def Name(cls):
    return 'memory.memory_health_quick'

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # TODO(crbug.com/586148): Benchmark should not depend on DeskClock app.
    return not possible_browser.platform.CanLaunchApplication(
        'com.google.android.deskclock')


# Benchmark is disabled by default because it takes too long to run.
@benchmark.Disabled('all')
class MemoryHealthPlan(MemoryHealthQuick):
  """Timeline based benchmark for the Memory Health Plan (5 iterations)."""
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'memory.memory_health_plan'


# TODO(bashi): Workaround for http://crbug.com/532075
# @benchmark.Enabled('android') shouldn't be needed.
@benchmark.Enabled('android')
class RendererMemoryBlinkMemoryMobile(_MemoryInfra):
  """Timeline based benchmark for measuring memory consumption on mobile
  sites on which blink's memory consumption is relatively high."""

  _RE_RENDERER_VALUES = re.compile('memory_.+_renderer')

  page_set = page_sets.BlinkMemoryMobilePageSet

  def SetExtraBrowserOptions(self, options):
    super(RendererMemoryBlinkMemoryMobile, self).SetExtraBrowserOptions(
        options)
    options.AppendExtraBrowserArgs([
        # Ignore certs errors because record_wpr cannot handle certs correctly
        # in some cases (e.g. WordPress).
        '--ignore-certificate-errors',
    ])

  @classmethod
  def Name(cls):
    return 'memory.blink_memory_mobile'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    return bool(cls._RE_RENDERER_VALUES.match(value.name))


# Disabled on reference builds because they don't support the new
# Tracing.requestMemoryDump DevTools API. See http://crbug.com/540022.
@benchmark.Disabled('reference')
class MemoryBenchmarkTop10Mobile(_MemoryInfra):
  """Timeline based benchmark for measuring memory on top 10 mobile sites."""

  page_set = page_sets.MemoryInfraTop10MobilePageSet

  @classmethod
  def Name(cls):
    return 'memory.top_10_mobile'


# Disabled on reference builds because they don't support the new
# Tracing.requestMemoryDump DevTools API.
# For 'reference' see http://crbug.com/540022.
# For 'android' see http://crbug.com/579546.
@benchmark.Disabled('reference', 'android')
class MemoryLongRunningIdleGmailTBM(_MemoryInfra):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page """
  page_set = page_sets.LongRunningIdleGmailPageSet

  def CreateTimelineBasedMeasurementOptions(self):
    v8_categories = [
        'blink.console', 'renderer.scheduler', 'v8', 'webkit.console']
    memory_categories = 'blink.console,disabled-by-default-memory-infra'
    category_filter = tracing_category_filter.TracingCategoryFilter(
        memory_categories)
    for category in v8_categories:
      category_filter.AddIncludedCategory(category)
    options = timeline_based_measurement.Options(category_filter)
    return options

  def SetupBenchmarkDefaultTraceRerunOptions(self, tbm_options):
    tbm_options.SetLegacyTimelineBasedMetrics((
        v8_gc_latency.V8GCLatency(),
        memory_timeline.MemoryTimelineMetric(),
    ))

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail_tbm'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True

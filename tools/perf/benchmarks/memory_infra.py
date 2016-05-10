# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from core import perf_benchmark

from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import memory_timeline

import page_sets


class _MemoryInfra(perf_benchmark.PerfBenchmark):
  """Base class for new-generation memory benchmarks based on memory-infra.

  This benchmark records data using memory-infra (https://goo.gl/8tGc6O), which
  is part of chrome tracing, and extracts it using timeline-based measurements.
  """

  # Subclasses can override this to use TBMv2 instead of TBMv1.
  TBM_VERSION = 1

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
    if self.TBM_VERSION == 1:
      # TBMv1 (see telemetry/telemetry/web_perf/metrics/memory_timeline.py
      # in third_party/catapult).
      tbm_options.SetLegacyTimelineBasedMetrics((
          memory_timeline.MemoryTimelineMetric(),
      ))
    elif self.TBM_VERSION == 2:
      # TBMv2 (see tracing/tracing/metrics/system_health/memory_metric.html
      # in third_party/catapult).
      tbm_options.SetTimelineBasedMetric('memoryMetric')
    else:
      raise Exception('Unrecognized TBM version: %s' % self.TBM_VERSION)
    return tbm_options


# TODO(crbug.com/606361): Remove benchmark when replaced by the TBMv2 version.
@benchmark.Disabled('all')
class MemoryHealthPlan(_MemoryInfra):
  """Timeline based benchmark for the Memory Health Plan."""
  page_set = page_sets.MemoryHealthStory
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'memory.memory_health_plan'

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # TODO(crbug.com/586148): Benchmark should not depend on DeskClock app.
    return not possible_browser.platform.CanLaunchApplication(
        'com.google.android.deskclock')


@benchmark.Enabled('android')
class TBMv2MemoryBenchmarkTop10Mobile(MemoryHealthPlan):
  """Timeline based benchmark for the Memory Health Plan based on TBMv2.

  This is a temporary benchmark to compare the new TBMv2 memory metric
  (memory_metric.html) with the existing TBMv1 one (memory_timeline.py). Once
  all issues associated with the TBMv2 metric are resolved, all memory
  benchmarks (including the ones in this file) will switch to use it instead
  of the TBMv1 metric and this temporary benchmark will be removed. See
  crbug.com/60361.
  """
  TBM_VERSION = 2

  @classmethod
  def Name(cls):
    return 'memory.top_10_mobile_tbmv2'


# Benchmark is disabled by default because it takes too long to run.
@benchmark.Disabled('all')
class DualBrowserBenchmark(_MemoryInfra):
  """Measures memory usage while interacting with two different browsers.

  The user story involves going back and forth between doing Google searches
  on a webview-based browser (a stand in for the Search app), and loading
  pages on a select browser.
  """
  TBM_VERSION = 2
  page_set = page_sets.DualBrowserStorySet
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'memory.dual_browser_test'


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


class _MemoryV8Benchmark(_MemoryInfra):
  def CreateTimelineBasedMeasurementOptions(self):
    v8_categories = [
        'blink.console', 'renderer.scheduler', 'v8', 'webkit.console']
    memory_categories = ['blink.console', 'disabled-by-default-memory-infra']
    category_filter = tracing_category_filter.TracingCategoryFilter(
        ','.join(['-*'] + v8_categories + memory_categories))
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetric('v8AndMemoryMetrics')
    return options


class MemoryLongRunningIdleGmail(_MemoryV8Benchmark):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page """
  page_set = page_sets.LongRunningIdleGmailPageSet

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail_tbmv2'


class MemoryLongRunningIdleGmailBackground(_MemoryV8Benchmark):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page """
  page_set = page_sets.LongRunningIdleGmailBackgroundPageSet

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail_background_tbmv2'

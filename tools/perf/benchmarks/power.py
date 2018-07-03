# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import power
import page_sets
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


class _PowerBenchmark(perf_benchmark.PerfBenchmark):
  """A benchmark that indirectly measures power through CPU time."""

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='toplevel')
    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_chrome_trace = True
    options.config.enable_cpu_trace = True
    options.SetTimelineBasedMetrics(['cpuTimeMetric'])
    return options


@benchmark.Info(emails=['perezju@chromium.org'])
class PowerTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'


# TODO(charliea): Delete this benchmark now that we're no longer measuring
# ground-truth power.
@benchmark.Info(emails=['charliea@chromium.org'])
class IdlePlatformBenchmark(perf_benchmark.PerfBenchmark):
  """Idle platform benchmark.

  This benchmark just starts up tracing agents and lets the platform sit idle.
  Our power benchmarks are prone to noise caused by other things running on the
  system. This benchmark is intended to help find the sources of noise.
  """
  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options(
        chrome_trace_category_filter.ChromeTraceCategoryFilter())
    options.config.enable_cpu_trace = True
    # Atrace tracing agent autodetects if its android and only runs if it is.
    options.config.enable_atrace_trace = True
    options.config.enable_chrome_trace = False
    options.SetTimelineBasedMetrics([
        'tracingMetric'
    ])
    return options

  def CreateStorySet(self, options):
    return page_sets.IdleStorySet()

  @classmethod
  def Name(cls):
    return 'power.idle_platform'


@benchmark.Info(emails=['charliea@chromium.org'])
class PowerDesktop(_PowerBenchmark):
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.DesktopPowerStorySet()

  @classmethod
  def Name(cls):
    return 'power.desktop'

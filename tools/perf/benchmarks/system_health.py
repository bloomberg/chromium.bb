# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
import page_sets

class _SystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  TRACING_CATEGORIES = [
    'benchmark',
    'navigation',
    'blink.user_timing',
  ]

  def CreateTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.SetTracingCategoryFilter(
        tracing_category_filter.TracingCategoryFilter(','.join(
            self.TRACING_CATEGORIES)))
    options.SetTimelineBasedMetric('SystemHealthMetrics')
    return options

  @classmethod
  def ShouldDisable(cls, browser):
    # http://crbug.com/600463
    galaxy_s5_type_name = 'SM-G900H'
    return browser.platform.GetDeviceTypeName() == galaxy_s5_type_name


@benchmark.Disabled('all')  # crbug.com/613050
class SystemHealthTop25(_SystemHealthBenchmark):
  page_set = page_sets.Top25PageSet

  @classmethod
  def Name(cls):
    return 'system_health.top25'

@benchmark.Disabled('android')  # crbug.com/601953
@benchmark.Disabled('all')  # crbug.com/613050
class SystemHealthKeyMobileSites(_SystemHealthBenchmark):
  page_set = page_sets.KeyMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'system_health.key_mobile_sites'


class _MemorySystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Chrome Memory System Health Benchmark.

  https://goo.gl/Jek2NL.
  """

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        # TODO(perezju): Temporary workaround to disable periodic memory dumps.
        # See: http://crbug.com/513692
        '--enable-memory-benchmarking',
    ])

  def CreateTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options(
        tracing_category_filter.TracingCategoryFilter(
            '-*,disabled-by-default-memory-infra'))
    options.config.enable_android_graphics_memtrack = True
    options.SetTimelineBasedMetric('memoryMetric')
    return options

  @classmethod
  def Name(cls):
    return 'system_health.memory_%s' % cls.page_set.PLATFORM

# https://github.com/catapult-project/catapult/issues/2340
@benchmark.Disabled('all')
class DesktopMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Desktop Chrome Memory System Health Benchmark."""
  page_set = page_sets.DesktopMemorySystemHealthStorySet

  @classmethod
  def ShouldDisable(cls, possible_browser):
    return possible_browser.platform.GetDeviceTypeName() != 'Desktop'

# https://github.com/catapult-project/catapult/issues/2340
@benchmark.Disabled('all')
class MobileMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Mobile Chrome Memory System Health Benchmark."""
  page_set = page_sets.MobileMemorySystemHealthStorySet

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # http://crbug.com/612144 (reference on Nexus 5X).
    return possible_browser.platform.GetDeviceTypeName() == 'Desktop' or (
        possible_browser.browser_type == 'reference' and
        possible_browser.platform.GetDeviceTypeName() == 'Nexus 5X')

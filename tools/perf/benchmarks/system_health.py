# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
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


class SystemHealthTop25(_SystemHealthBenchmark):
  page_set = page_sets.Top25PageSet

  @classmethod
  def Name(cls):
    return 'system_health.top25'


class SystemHealthKeyMobileSites(_SystemHealthBenchmark):
  page_set = page_sets.KeyMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'system_health.key_mobile_sites'


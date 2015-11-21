# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
import page_sets

from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import startup


class _StartWithUrlTBM(perf_benchmark.PerfBenchmark):
  """Measures time to start Chrome with startup URLs."""

  page_set = page_sets.StartupPagesPageSetTBM

  @classmethod
  def Name(cls):
    # TODO(gabadie): change to start_with_url.* after confirming that both
    # benchmarks produce the same results.
    return 'start_with_url2.startup_pages'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-stats-collection-bindings'
    ])

  def CreateTimelineBasedMeasurementOptions(self):
    startup_category_filter = tracing_category_filter.TracingCategoryFilter(
        filter_string='startup,blink.user_timing')
    options = timeline_based_measurement.Options(
        overhead_level=startup_category_filter)
    options.SetTimelineBasedMetrics(
        [startup.StartupTimelineMetric()])
    return options


@benchmark.Enabled('has tabs')
@benchmark.Enabled('android')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlColdTBM(_StartWithUrlTBM):
  """Measures time to start Chrome cold with startup URLs."""

  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
      options.clear_sytem_cache_for_browser_and_profile_on_start = True
      super(StartWithUrlColdTBM, self).SetExtraBrowserOptions(options)

  @classmethod
  def Name(cls):
    return 'start_with_url2.cold.startup_pages'


@benchmark.Enabled('has tabs')
@benchmark.Enabled('android')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlWarmTBM(_StartWithUrlTBM):
  """Measures stimetime to start Chrome warm with startup URLs."""

  options = {'pageset_repeat': 10}

  @classmethod
  def Name(cls):
    return 'start_with_url2.warm.startup_pages'

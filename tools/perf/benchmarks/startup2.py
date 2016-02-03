# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
import page_sets

from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import startup


class _StartupPerfBenchmark(perf_benchmark.PerfBenchmark):
  """Measures time to start Chrome."""

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


# TODO(gabadie): Replaces startup.* by startup2.* after confirming that both
# benchmarks produce the same results.


@benchmark.Disabled('snowleopard')  # crbug.com/336913
@benchmark.Disabled('android')
class StartupColdBlankPage2(_StartupPerfBenchmark):
  """Measures cold startup time with a clean profile."""

  page_set = page_sets.BlankPageSetTBM
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'startup2.cold.blank_page'

  def SetExtraBrowserOptions(self, options):
    options.clear_sytem_cache_for_browser_and_profile_on_start = True
    super(StartupColdBlankPage2, self).SetExtraBrowserOptions(options)


@benchmark.Disabled('android')
class StartupWarmBlankPage2(_StartupPerfBenchmark):
  """Measures warm startup time with a clean profile."""

  page_set = page_sets.BlankPageSetTBM
  options = {'pageset_repeat': 20}

  @classmethod
  def Name(cls):
    return 'startup2.warm.blank_page'

  @classmethod
  def ValueCanBeAddedPredicate(cls, _, is_first_result):
    # Ignores first results because the first invocation is actualy cold since
    # we are loading the profile for the first time.
    return not is_first_result

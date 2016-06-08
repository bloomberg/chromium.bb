# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler v2.

For details, see design doc:
https://docs.google.com/document/d/1EZQX-x3eEphXupiX-Hq7T4Afju5_sIdxPWYetj7ynd0
"""

from core import perf_benchmark
import page_sets

from telemetry import benchmark
from telemetry.page import cache_temperature
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement


class _PageCyclerV2(perf_benchmark.PerfBenchmark):
  def CreateTimelineBasedMeasurementOptions(self):
    cat_filter = tracing_category_filter.TracingCategoryFilter(
        filter_string='*,blink.console,navigation,blink.user_timing,loading')

    tbm_options = timeline_based_measurement.Options(
        overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetric('firstPaintMetric')
    return tbm_options


@benchmark.Disabled('win')  # crbug.com/615178
class PageCyclerV2Typical25(_PageCyclerV2):
  """Page load time benchmark for a 25 typical web pages.

  Designed to represent typical, not highly optimized or highly popular web
  sites. Runs against pages recorded in June, 2014.
  """
  options = {'pageset_repeat': 2}

  @classmethod
  def Name(cls):
    return 'page_cycler_v2.typical_25'

  @classmethod
  def ShouldDisable(cls, possible_browser):  # crbug.com/615178
    if (possible_browser.browser_type == 'reference' and
        possible_browser.platform.GetOSName() == 'android'):
      return True
    # crbug.com/616781
    if (cls.IsSvelte(possible_browser) or
        possible_browser.platform.GetDeviceTypeName() == 'Nexus 5X' or
        possible_browser.platform.GetDeviceTypeName() == 'AOSP on BullHead'):
      return True
    return False

  def CreateStorySet(self, options):
    return page_sets.Typical25PageSet(run_no_page_interactions=True,
        cache_temperatures=[
          cache_temperature.PCV1_COLD, cache_temperature.PCV1_WARM])

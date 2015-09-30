# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement

import page_sets


TIMELINE_REQUIRED_CATEGORY = 'blink.console'


class TbmSmoke(perf_benchmark.PerfBenchmark):
  """Timeline based measurement benchmark to test TBM Everywhere."""
  # TODO(eakuefner): Remove this benchmark once crbug.com/461101 is closed.

  page_set = page_sets.BlankPageSetForTbmSmoke

  def CreateTimelineBasedMeasurementOptions(self):
    cat_filter = tracing_category_filter.CreateMinimalOverheadFilter()
    cat_filter.AddIncludedCategory(TIMELINE_REQUIRED_CATEGORY)

    return timeline_based_measurement.Options(
        overhead_level=cat_filter)

  @classmethod
  def Name(cls):
    return 'tbm_smoke.tbm_smoke'

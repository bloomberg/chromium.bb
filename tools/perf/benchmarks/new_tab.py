# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry import benchmark
from telemetry.web_perf import timeline_based_measurement

import page_sets


@benchmark.Disabled('android')
class NewTabPage(perf_benchmark.PerfBenchmark):
  """Timeline based measurement benchmark for the New Tab Page."""
  page_set = page_sets.NewTabPagePageSet

  def CreateTimelineBasedMeasurementOptions(self):
    return timeline_based_measurement.Options(
        overhead_level=timeline_based_measurement.MINIMAL_OVERHEAD_LEVEL)

  @classmethod
  def Name(cls):
    return 'new_tab.new_tab_page'

  # TODO(beaudoin): Define ValueCanBeAddedPredicate to filter out things we
  # don't care for.

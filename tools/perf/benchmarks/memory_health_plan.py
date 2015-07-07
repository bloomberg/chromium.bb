# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry import benchmark
from telemetry.core.platform import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement

import page_sets


@benchmark.Enabled('android')
class MemoryHealthPlan(perf_benchmark.PerfBenchmark):
  """Timeline based benchmark for the Memory Health Plan."""

  page_set = page_sets.MemoryHealthStory

  def CreateTimelineBasedMeasurementOptions(self):
    trace_memory = tracing_category_filter.TracingCategoryFilter(
      filter_string='disabled-by-default-memory-infra')
    return timeline_based_measurement.Options(overhead_level=trace_memory)

  @classmethod
  def Name(cls):
    return 'memory.memory_health_plan'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    return value.name.startswith('measurement-memory_')

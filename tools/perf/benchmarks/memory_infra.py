# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from core import perf_benchmark

from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement

import page_sets


class _MemoryInfra(perf_benchmark.PerfBenchmark):
  """Base class for new-generation memory benchmarks based on memory-infra.

  This benchmark records data using memory-infra (https://goo.gl/8tGc6O), which
  is part of chrome tracing, and extracts it using timeline-based measurements.
  """

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        # TODO(perezju): Temporary workaround to disable periodic memory dumps.
        # See: http://crbug.com/513692
        '--enable-memory-benchmarking',
        # TODO(ssid): Remove this flag after fixing http://crbug.com/461788.
        '--no-sandbox'
    ])

  def CreateTimelineBasedMeasurementOptions(self):
    # Enable only memory-infra, to get memory dumps, and blink.console, to get
    # the timeline markers used for mapping threads to tabs.
    trace_memory = tracing_category_filter.TracingCategoryFilter(
      filter_string='-*,blink.console,disabled-by-default-memory-infra')
    options = timeline_based_measurement.Options(overhead_level=trace_memory)
    options.tracing_options.enable_android_graphics_memtrack = True
    return options


# TODO(bashi): Workaround for http://crbug.com/532075
# @benchmark.Enabled('android') shouldn't be needed.
@benchmark.Enabled('android')
class MemoryHealthPlan(_MemoryInfra):
  """Timeline based benchmark for the Memory Health Plan."""

  page_set = page_sets.MemoryHealthStory

  @classmethod
  def Name(cls):
    return 'memory.memory_health_plan'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    return (value.tir_label in ['foreground', 'background']
            and value.name.startswith('memory_')
            and not 'allocated_objects_' in value.name)


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


# Disabled on reference builds because they don't support the new
# Tracing.requestMemoryDump DevTools API. See http://crbug.com/540022.
@benchmark.Disabled('reference')
class MemoryBenchmarkTop10Mobile(_MemoryInfra):
  """Timeline based benchmark for measuring memory on top 10 mobile sites."""

  page_set = page_sets.MemoryInfraTop10MobilePageSet

  @classmethod
  def Name(cls):
    return 'memory.top_10_mobile'

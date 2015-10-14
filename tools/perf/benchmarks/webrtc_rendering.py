# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement

import page_sets

BENCHMARK_VALUES = 'WebRTCRendering_'

class WebRTCRendering(perf_benchmark.PerfBenchmark):
  """Timeline based benchmark for the WebRtc rendering."""

  page_set = page_sets.WebrtcRenderingPageSet

  def CreateTimelineBasedMeasurementOptions(self):
    cc_filter = tracing_category_filter.TracingCategoryFilter(
        filter_string='webrtc,webkit.console,blink.console')
    return timeline_based_measurement.Options(overhead_level=cc_filter)

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--use-fake-device-for-media-stream')
    options.AppendExtraBrowserArgs('--use-fake-ui-for-media-stream')

  @classmethod
  def Name(cls):
    return 'webrtc_rendering.webrtc_rendering'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    return value.name.startswith(BENCHMARK_VALUES)

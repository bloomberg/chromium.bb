# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

import page_sets
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Owner(emails=['qiangchen@chromium.org', # For smoothness metrics
                         'ehmaldonado@chromium.org',
                         'phoglund@chromium.org'])
class WebrtcPerfBenchmark(perf_benchmark.PerfBenchmark):
  """Base class for WebRTC metrics for real-time communications tests."""
  page_set = page_sets.WebrtcPageSet

  @classmethod
  def Name(cls):
    return 'webrtc'

  def CreateTimelineBasedMeasurementOptions(self):
    categories = [
        # Disable all categories by default.
        '-*',
        'toplevel',
        # WebRTC
        'webrtc',
    ]

    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string=','.join(categories))
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics([
        'cpuTimeMetric',
        'webrtcRenderingMetric',
    ])
    return options

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--use-fake-device-for-media-stream')
    options.AppendExtraBrowserArgs('--use-fake-ui-for-media-stream')

  def GetExpectations(self):
    return page_sets.WebrtcExpectations()

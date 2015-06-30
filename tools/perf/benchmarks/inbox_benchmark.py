# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry import benchmark
from telemetry.web_perf import timeline_based_measurement

from page_sets import inbox


@benchmark.Disabled  # http://crbug.com/452257
class Inbox(perf_benchmark.PerfBenchmark):
  """Runs the timeline based measurement against inbox pageset."""
  test = timeline_based_measurement.TimelineBasedMeasurement

  def CreateStorySet(self, options):
    return inbox.InboxPageSet()

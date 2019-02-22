# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from measurements import thread_times


class _ThreadTimes(perf_benchmark.PerfBenchmark):

  @classmethod
  def Name(cls):
    return 'thread_times'

  @classmethod
  def ShouldAddValue(cls, name, from_first_story_run):
    del from_first_story_run  # unused
    # Default to only reporting per-frame metrics.
    return 'per_second' not in name

  def CreatePageTest(self, options):
    return thread_times.ThreadTimes(report_silk_details=True)

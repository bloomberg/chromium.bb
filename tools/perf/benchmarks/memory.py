# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import memory
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class MemoryMobile(perf_benchmark.PerfBenchmark):
  test = memory.Memory
  page_set = page_sets.MobileMemoryPageSet

  @classmethod
  def Name(cls):
    return 'memory.mobile_memory'


# Disable on yosemite due to crbug.com/517806
# Disable on reference due to crbug.com/539728
# Disable on all Mac as it's also failing on 10.11 and retina.
# crbug.com/555045
@benchmark.Disabled('mac', 'reference')
class MemoryTop7Stress(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption."""
  test = memory.Memory
  page_set = page_sets.Top7StressPageSet

  @classmethod
  def Name(cls):
    return 'memory.top_7_stress'

  @classmethod
  def ShouldDisable(cls, possible_browser):
    return cls.IsSvelte(possible_browser)  # http://crbug.com/555092


@benchmark.Disabled('android')  # crbug.com/542682
class MemoryLongRunningIdleGmailBackground(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page in background tab"""
  test = memory.Memory
  page_set = page_sets.LongRunningIdleGmailBackgroundPageSet

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail_background'

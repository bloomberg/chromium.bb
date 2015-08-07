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


@benchmark.Disabled('yosemite')  # crbug.com/517806
class MemoryTop7Stress(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption."""
  test = memory.Memory
  page_set = page_sets.Top7StressPageSet

  @classmethod
  def Name(cls):
    return 'memory.top_7_stress'


@benchmark.Enabled('has tabs')
@benchmark.Disabled('android') # Benchmark uses > 700MB of memory.
class MemoryIdleMultiTab(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption
  with many tabs and idle times. """
  test = memory.Memory
  page_set = page_sets.IdleMultiTabCasesPageSet

  def SetExtraBrowserOptions(self, options):
    # This benchmark opens tabs from JavaScript, which does not work
    # with popup-blocking enabled.
    options.AppendExtraBrowserArgs(['--disable-popup-blocking'])

  @classmethod
  def Name(cls):
    return 'memory.idle_multi_tab'


class MemoryLongRunningIdleGmail(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page """
  test = memory.Memory
  page_set = page_sets.LongRunningIdleGmailPageSet

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail'


class MemoryLongRunningIdleGmailBackground(perf_benchmark.PerfBenchmark):
  """Use (recorded) real world web sites and measure memory consumption
  of long running idle Gmail page in background tab"""
  test = memory.Memory
  page_set = page_sets.LongRunningIdleGmailBackgroundPageSet

  @classmethod
  def Name(cls):
    return 'memory.long_running_idle_gmail_background'
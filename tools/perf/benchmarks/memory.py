# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from measurements import memory
import page_sets


@benchmark.Enabled('android')
class MemoryMobile(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.MobileMemoryPageSet

  @classmethod
  def Name(cls):
    return 'memory.mobile_memory'


class MemoryTop7Stress(benchmark.Benchmark):
  """Use (recorded) real world web sites and measure memory consumption."""
  test = memory.Memory
  page_set = page_sets.Top7StressPageSet

  @classmethod
  def Name(cls):
    return 'memory.top_7_stress'


class MemoryTop7StressWithSlimmingPaint(benchmark.Benchmark):
  """Use (recorded) real world web sites and measure memory consumption,
  with --enable--slimming-paint."""

  test = memory.Memory
  page_set = page_sets.Top7StressPageSet

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-slimming-paint'])

  @classmethod
  def Name(cls):
    return 'memory.top_7_stress_slimming_paint'


@benchmark.Enabled('has tabs')
@benchmark.Disabled('android')  # Benchmark uses > 700MB of memory.
class MemoryIdleMultiTab(benchmark.Benchmark):
  """Use (recorded) real world web sites and measure memory consumption
  with many tabs and idle times. """
  test = memory.Memory
  page_set = page_sets.IdleMultiTabCasesPageSet

  def CustomizeBrowserOptions(self, options):
    # This benchmark opens tabs from JavaScript, which does not work
    # with popup-blocking enabled.
    options.AppendExtraBrowserArgs(['--disable-popup-blocking'])

  @classmethod
  def Name(cls):
    return 'memory.idle_multi_tab'
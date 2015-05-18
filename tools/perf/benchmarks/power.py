# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from benchmarks import silk_flags
from measurements import power
import page_sets


@benchmark.Enabled('android')
class PowerAndroidAcceptance(benchmark.Benchmark):
  """Android power acceptance test."""
  test = power.Power
  page_set = page_sets.AndroidAcceptancePageSet
  @classmethod
  def Name(cls):
    return 'power.android_acceptance'


@benchmark.Enabled('android')
class PowerTypical10Mobile(benchmark.Benchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet
  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'

@benchmark.Enabled('android')
class PowerGpuRasterizationTypical10Mobile(benchmark.Benchmark):
  """Measures power on key mobile sites with GPU rasterization."""
  tag = 'gpu_rasterization'
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'power.gpu_rasterization.typical_10_mobile'

@benchmark.Enabled('mac')
class PowerTop10(benchmark.Benchmark):
  """Top 10 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top10PageSet
  @classmethod
  def Name(cls):
    return 'power.top_10'


@benchmark.Disabled()  # crbug.com/489214
class PowerTop25(benchmark.Benchmark):
  """Top 25 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top25PageSet
  @classmethod
  def Name(cls):
    return 'power.top_25'

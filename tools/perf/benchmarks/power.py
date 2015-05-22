# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from benchmarks import silk_flags
from measurements import power
from telemetry import benchmark
import page_sets


@benchmark.Enabled('android')
class PowerAndroidAcceptance(perf_benchmark.PerfBenchmark):
  """Android power acceptance test."""
  test = power.Power
  page_set = page_sets.AndroidAcceptancePageSet
  @classmethod
  def Name(cls):
    return 'power.android_acceptance'


@benchmark.Enabled('android')
class PowerTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet
  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'

@benchmark.Enabled('android')
class PowerGpuRasterizationTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Measures power on key mobile sites with GPU rasterization."""
  tag = 'gpu_rasterization'
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'power.gpu_rasterization.typical_10_mobile'

@benchmark.Enabled('mac')
class PowerTop10(perf_benchmark.PerfBenchmark):
  """Top 10 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top10PageSet
  @classmethod
  def Name(cls):
    return 'power.top_10'


@benchmark.Enabled('mac')
class PowerTop25(perf_benchmark.PerfBenchmark):
  """Top 25 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top25PageSet
  @classmethod
  def Name(cls):
    return 'power.top_25'

  def CreateUserStorySet(self, _):
    # Exclude techcrunch.com. It is not suitable for this benchmark because it
    # does not consistently become quiescent within 60 seconds.
    user_stories = self.page_set()
    found = next((x for x in user_stories if 'techcrunch.com' in x.url), None)
    if found:
      user_stories.RemoveUserStory(found)
    return user_stories

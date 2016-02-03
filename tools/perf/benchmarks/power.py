# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from benchmarks import silk_flags
from measurements import power
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class PowerAndroidAcceptance(perf_benchmark.PerfBenchmark):
  """Android power acceptance test."""
  test = power.Power
  page_set = page_sets.AndroidAcceptancePageSet

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.android_acceptance'


@benchmark.Enabled('android')
class PowerTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'


@benchmark.Enabled('android')
@benchmark.Disabled('all')
class PowerTypical10MobileReload(perf_benchmark.PerfBenchmark):
  """Android typical 10 mobile power reload test."""
  test = power.LoadPower
  page_set = page_sets.Typical10MobileReloadPageSet

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile_reload'


@benchmark.Enabled('android')
class PowerGpuRasterizationTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Measures power on key mobile sites with GPU rasterization."""
  tag = 'gpu_rasterization'
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.gpu_rasterization.typical_10_mobile'

  @classmethod
  def ShouldDisable(cls, possible_browser):
    return cls.IsSvelte(possible_browser)  # http://crbug.com/563968


@benchmark.Enabled('mac')
class PowerTop10(perf_benchmark.PerfBenchmark):
  """Top 10 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top10PageSet

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.top_10'


@benchmark.Enabled('mac')
class PowerGpuRasterizationTop10(perf_benchmark.PerfBenchmark):
  """Top 10 quiescent power test with GPU rasterization enabled."""
  tag = 'gpu_rasterization'
  test = power.QuiescentPower
  page_set = page_sets.Top10PageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.gpu_rasterization.top_10'


@benchmark.Enabled('mac')
class PowerTop25(perf_benchmark.PerfBenchmark):
  """Top 25 quiescent power test."""
  test = power.QuiescentPower
  page_set = page_sets.Top25PageSet

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.top_25'

  def CreateStorySet(self, _):
    # Exclude techcrunch.com. It is not suitable for this benchmark because it
    # does not consistently become quiescent within 60 seconds.
    stories = self.page_set()
    found = next((x for x in stories if 'techcrunch.com' in x.url), None)
    if found:
      stories.RemoveStory(found)
    return stories


@benchmark.Enabled('mac')
class PowerGpuRasterizationTop25(perf_benchmark.PerfBenchmark):
  """Top 25 quiescent power test with GPU rasterization enabled."""
  tag = 'gpu_rasterization'
  test = power.QuiescentPower
  page_set = page_sets.Top25PageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.gpu_rasterization.top_25'

  def CreateStorySet(self, _):
    # Exclude techcrunch.com. It is not suitable for this benchmark because it
    # does not consistently become quiescent within 60 seconds.
    stories = self.page_set()
    found = next((x for x in stories if 'techcrunch.com' in x.url), None)
    if found:
      stories.RemoveStory(found)
    return stories


@benchmark.Enabled('linux', 'mac', 'win', 'chromeos')
class PowerPPSControlDisabled(perf_benchmark.PerfBenchmark):
  """A single page with a small-ish non-essential plugin. In this test, Plugin
  Power Saver (PPS) is disabled, so the plugin should continue animating and
  taking power."""
  test = power.QuiescentPower
  page_set = page_sets.PluginPowerSaverPageSet
  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--disable-plugin-power-saver'])
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.pps_control_disabled'


@benchmark.Enabled('linux', 'mac', 'win', 'chromeos')
class PowerPPSControlEnabled(perf_benchmark.PerfBenchmark):
  """A single page with a small-ish non-essential plugin. In this test, Plugin
  Power Saver (PPS) is enabled, so the plugin should be throttled (idle with a
  "Click to play" button)."""
  test = power.QuiescentPower
  page_set = page_sets.PluginPowerSaverPageSet
  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-plugin-power-saver'])
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.pps_control_enabled'


@benchmark.Enabled('linux', 'mac', 'win', 'chromeos')
class PowerThrottledPlugins(perf_benchmark.PerfBenchmark):
  """Tests that pages with flash ads take more power without Plugin Power Saver
  (PPS) throttling them."""
  test = power.QuiescentPower
  page_set = page_sets.ThrottledPluginsPageSet
  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--disable-plugin-power-saver'])
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.throttled_plugins_pps_disabled'


@benchmark.Enabled('linux', 'mac', 'win', 'chromeos')
class PowerThrottledPluginsPPS(perf_benchmark.PerfBenchmark):
  """Tests that pages with flash ads take less power with Plugin Power Saver
  (PPS) enabled to throttle them."""
  test = power.QuiescentPower
  page_set = page_sets.ThrottledPluginsPageSet
  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-plugin-power-saver'])
    options.full_performance_mode = False

  @classmethod
  def Name(cls):
    return 'power.throttled_plugins_pps_enabled'

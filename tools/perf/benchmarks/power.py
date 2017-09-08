# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import power
import page_sets
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Owner(emails=['perezju@chromium.org'])
class PowerTypical10Mobile(perf_benchmark.PerfBenchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    options.full_performance_mode = False

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # http://crbug.com/597656
    if (possible_browser.browser_type == 'reference' and
        possible_browser.platform.GetDeviceTypeName() == 'Nexus 5X'):
      return True

    # crbug.com/671631
    return possible_browser.platform.GetDeviceTypeName() == 'Nexus 9'

  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass
    return StoryExpectations()


@benchmark.Owner(emails=['erikchen@chromium.org'])
class PowerScrollingTrivialPage(perf_benchmark.PerfBenchmark):
  """Measure power consumption for some very simple pages."""
  test = power.QuiescentPower
  page_set = page_sets.TrivialSitesStorySet
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MAC]

  @classmethod
  def Name(cls):
    return 'power.trivial_pages'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass
    return StoryExpectations()


class PowerSteadyStatePages(perf_benchmark.PerfBenchmark):
  """Measure power consumption for real web sites in steady state (no user
  interactions)."""
  test = power.QuiescentPower
  page_set = page_sets.IdleAfterLoadingStories
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MAC]

  @classmethod
  def Name(cls):
    return 'power.steady_state'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('http://abcnews.go.com/', [story.expectations.ALL],
                          'crbug.com/505990')
    return StoryExpectations()


class IdlePlatformBenchmark(perf_benchmark.PerfBenchmark):
  """Idle platform benchmark.

  This benchmark just starts up tracing agents and lets the platform sit idle.
  Our power benchmarks are prone to noise caused by other things running on the
  system. This benchmark is intended to help find the sources of noise.
  """
  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options(
        chrome_trace_category_filter.ChromeTraceCategoryFilter())
    options.config.enable_battor_trace = True
    options.config.enable_cpu_trace = True
    # Atrace tracing agent autodetects if its android and only runs if it is.
    options.config.enable_atrace_trace = True
    options.config.enable_chrome_trace = False
    options.SetTimelineBasedMetrics([
        'clockSyncLatencyMetric',
        'powerMetric',
        'tracingMetric'
    ])
    return options

  @classmethod
  def ShouldDisable(cls, possible_browser):
    return not possible_browser.platform.HasBattOrConnected()

  def CreateStorySet(self, options):
    return page_sets.IdleStorySet()

  @classmethod
  def Name(cls):
    return 'power.idle_platform'

  def GetExpectations(self):
    class StoryExpectations(story.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing disabled.
    return StoryExpectations()


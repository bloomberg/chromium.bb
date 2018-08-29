# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core import perf_benchmark

from contrib.cros_benchmarks import tab_switching_stories
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['vovoy@chromium.org'],
                 component='OS>Performance')
class CrosTabSwitchingTypical24(perf_benchmark.PerfBenchmark):
  """Measures tab switching performance with 24 tabs.

  The script opens 24 pages in 24 different tabs, waits for them to load,
  and then switches to each tabs and measures the tabs paint time. The 24
  pages were chosen from Alexa top ranking sites. Tab paint time is the
  time between tab being requested and the first paint event.

  Benchmark specific option:
    --tabset-repeat=N: Duplicate tab set for N times.
    --pause-after-creation=N: Pauses N secs after tab creation.
    --pause-after-switch=N: Pauses N secs after each tab switch.
  The following usage example opens 120 tabs.
  $ ./run_benchmark --browser=cros-chrome --remote=DUT_IP
  cros_tab_switching.typical_24 --tabset-repeat=5
  """
  SUPPORTED_PLATFORMS = [story.expectations.ALL_CHROMEOS]


  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--tabset-repeat', type='int', default=1,
                      help='repeat tab page set')
    parser.add_option('--pause-after-creation', type='int', default=0,
                      help='pause between tab creation and tab switch')
    parser.add_option('--pause-after-switch', type='int', default=3,
                      help='pause between each tab switch')

  def CreateStorySet(self, options):
    # When running this test with test_that, fetch_benchmark_deps.py would
    # invoke this method. Options doesn't have attribute cros_remote in this
    # case.
    cros_remote = getattr(options, 'cros_remote', None)
    story_set = story.StorySet(
        archive_data_file='data/tab_switching.json',
        base_dir=os.path.dirname(os.path.abspath(__file__)),
        cloud_storage_bucket=story.PARTNER_BUCKET)
    story_set.AddStory(tab_switching_stories.CrosMultiTabTypical24Story(
        story_set, cros_remote, options.tabset_repeat,
        options.pause_after_creation, options.pause_after_switch))
    return story_set

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()
    category_filter.AddIncludedCategory('latency')
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics(['tabsMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'cros_tab_switching.typical_24'

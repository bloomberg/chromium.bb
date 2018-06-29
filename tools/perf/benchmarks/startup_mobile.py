# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging

from core import perf_benchmark

from telemetry.core import android_platform
from telemetry.internal.browser import browser_finder
from telemetry.timeline import chrome_trace_category_filter
from telemetry.util import wpr_modes
from telemetry.web_perf import timeline_based_measurement
from telemetry import benchmark
from telemetry import decorators
from telemetry import story as story_module

# The import error below is mysterious: it produces no detailed error message,
# while appending a proper sys.path does not help.
from devil.android.sdk import intent # pylint: disable=import-error

# Chrome Startup Benchmarks for mobile devices (running Android).
#
# This set of benchmarks (experimental.startup.mobile) aims to replace
# the benchmark experimental.startup.android.coldish. It brings two
# improvements:
# 1. a name that is more aligned with the end state :)
# 2. uses the new shared state pattern as recommended by:
#    third_party/catapult/telemetry/examples/benchmarks/android_go_benchmark.py
# The shared state allows starting the browser multiple times during a
# benchmark, properly evicting caches, populating caches, running as many
# iterations as needed without being affected by pageset-repeat being set by
# bisect/Pinpoint.
#
# Note: this benchmark is not yet ready for FYI bots because:
# 1. The requests are not routed through WPR yet.
# 2. The benchmark is not yet careful with evicting OS pagecache at proper
#    times.
# As soon as these two functions are implemented, we will replace
# 'experimental.startup.android.coldish' with 'experimental.startup.mobile'.
# After adding warm starts the resulting benchmark can replace the
# 'start_with_url.*' family that has aged quite a bit.
#
# The recommended way to run this benchmark is:
# shell> CHROMIUM_OUTPUT_DIR=gn_android/Release tools/perf/run_benchmark \
#   -v experimental.startup.mobile --browser=android-chrome \
#   --output-dir=/tmp/avoid-polluting-chrome-tree

class _MobileStartupSharedState(story_module.SharedState):

  def __init__(self, test, finder_options, story_set):
    """
    Args:
      test: opaquely passed to parent class constructor.
      finder_options: A BrowserFinderOptions object.
      story_set: opaquely passed to parent class constructor.
    """
    super(_MobileStartupSharedState, self).__init__(
        test, finder_options, story_set)
    self._finder_options = finder_options
    self._possible_browser = browser_finder.FindBrowser(self._finder_options)
    self._current_story = None

    # This is an Android-only shared state.
    assert isinstance(self.platform, android_platform.AndroidPlatform)
    self._finder_options.browser_options.browser_user_agent_type = 'mobile'

    # TODO(pasko): This will always use live sites. Should use options to
    # configure network_controller properly. See e.g.: https://goo.gl/nAsyFr
    self.platform.network_controller.Open(wpr_modes.WPR_OFF)
    self.platform.Initialize()

  @property
  def platform(self):
    return self._possible_browser.platform

  def TearDownState(self):
    self.platform.network_controller.Close()

  def LaunchBrowser(self, url):
    self.platform.FlushDnsCache()
    # TODO(crbug.com/811244): Determine whether this ensures the page cache is
    # cleared after |FlushOsPageCaches()| returns.
    self._possible_browser.FlushOsPageCaches()
    self.platform.StartActivity(
        intent.Intent(package=self._possible_browser.settings.package,
                      activity=self._possible_browser.settings.activity,
                      action=None, data=url),
        blocking=True)

  @contextlib.contextmanager
  def FindBrowser(self):
    """Find and manage the lifetime of a browser.

    The browser is closed when exiting the context managed code, and the
    browser state is dumped in case of errors during the story execution.
    """
    browser = self._possible_browser.FindExistingBrowser()
    try:
      yield browser
    except Exception as exc:
      logging.critical(
          '%s raised during story run. Dumping current browser state to help'
          ' diagnose this issue.', type(exc).__name__)
      browser.DumpStateUponFailure()
      raise
    finally:
      self.CloseBrowser(browser)

  def WillRunStory(self, story):
    # TODO(pasko): Should start replay to use WPR recordings.
    # See e.g.: https://goo.gl/UJuu8a
    self._possible_browser.SetUpEnvironment(
        self._finder_options.browser_options)
    self._current_story = story

  def RunStory(self, _):
    self._current_story.Run(self)

  def DidRunStory(self, _):
    self._current_story = None
    self._possible_browser.CleanUpEnvironment()

  def DumpStateUponFailure(self, story, results):
    del story
    del results
    # Note: Dumping state of objects upon errors, e.g. of the browser, is
    # handled individually by the context managers that handle their lifetime.

  def CanRunStory(self, _):
    return True

  def CloseBrowser(self, browser):
    # TODO(crbug.com/854212): This method includes some workarounds for bugs
    # that occur when closing the browser while tracing is running.  When the
    # linked bug is fixed, it should be possible to replace this with just
    # browser.Close().

    try:
      # a) Explicitly call flush tracing so that we retain a copy of the
      # trace from this browser before it's closed.
      if self.platform.tracing_controller.is_tracing_running:
        self.platform.tracing_controller.FlushTracing()

      # b) Close all tabs before closing the browser. Prevents a bug that
      # would cause future browser instances to hang when older tabs receive
      # DevTools requests.
      while len(browser.tabs) > 0:
        browser.tabs[0].Close(keep_one=False)
    finally:
      browser.Close()


class _MobileStartupWithIntentStory(story_module.Story):
  def __init__(self):
    super(_MobileStartupWithIntentStory, self).__init__(
        _MobileStartupSharedState, name='intent:coldish:bbc')
    self._action_runner = None

  def Run(self, state):
    for _ in xrange(10):
      state.LaunchBrowser('http://bbc.co.uk')
      with state.FindBrowser() as browser:
        action_runner = browser.foreground_tab.action_runner
        action_runner.tab.WaitForDocumentReadyStateToBeComplete()


class _MobileStartupStorySet(story_module.StorySet):
  def __init__(self):
    super(_MobileStartupStorySet, self).__init__()
    self.AddStory(_MobileStartupWithIntentStory())


# The mobile startup benchmark uses specifics of AndroidPlatform and hardcodes
# sending Android intents. The benchmark is disabled on non-Android to avoid
# failure at benchmark_smoke_unittest.BenchmarkSmokeTest.
@benchmark.Owner(emails=['pasko@chromium.org',
                         'chrome-android-perf-status@chromium.org'])
@decorators.Disabled('linux', 'mac', 'win')
class MobileStartupBenchmark(perf_benchmark.PerfBenchmark):

  # Set |pageset_repeat| to 1 to control the amount of iterations from the
  # stories. This would avoid setting per-story pageset_repeat at bisect time.
  options = {'pageset_repeat': 1}

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string=('navigation,loading,net,netlog,network,offline_pages,'
                'startup,toplevel,Java,EarlyJava'))

    options = timeline_based_measurement.Options(cat_filter)
    options.config.enable_chrome_trace = True
    options.SetTimelineBasedMetrics([
        'tracingMetric',
        'androidStartupMetric',
    ])
    return options

  def CreateStorySet(self, options):
    return _MobileStartupStorySet()

  @classmethod
  def Name(cls):
    return 'experimental.startup.mobile'

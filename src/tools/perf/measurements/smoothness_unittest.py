# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import decorators
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case
from telemetry.util import wpr_modes

from measurements import smoothness
from measurements import rendering_util


class FakeTracingController(object):

  def __init__(self):
    self.config = None

  def StartTracing(self, config):
    self.config = config

  def IsChromeTracingSupported(self):
    return True


class FakePlatform(object):

  def __init__(self):
    self.tracing_controller = FakeTracingController()


class FakeBrowser(object):

  def __init__(self):
    self.platform = FakePlatform()


class FakeTab(object):

  def __init__(self):
    self.browser = FakeBrowser()

  def CollectGarbage(self):
    pass

  def ExecuteJavaScript(self, js):
    pass


class SmoothnessUnitTest(page_test_test_case.PageTestTestCase):
  """Smoke test for smoothness measurement

     Runs smoothness measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  # crbug.com/483212 and crbug.com/713260
  @decorators.Disabled('chromeos', 'linux')
  def testSmoothness(self):
    ps = self.CreateStorySetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertFalse(results.had_failures)
    stat = rendering_util.ExtractStat(results)

    self.assertGreater(stat['frame_times'].mean, 0)
    self.assertGreaterEqual(stat['percentage_smooth'].mean, 0)

  @decorators.Enabled('android')  # SurfaceFlinger is android-only
  def testSmoothnessSurfaceFlingerMetricsCalculated(self):
    ps = self.CreateStorySetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertFalse(results.had_failures)
    stat = rendering_util.ExtractStat(results)

    self.assertGreater(stat['avg_surface_fps'].mean, 0)
    self.assertGreater(stat['jank_count'].mean, -1)
    self.assertGreater(stat['frame_lengths'].mean, 0)

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(smoothness.Smoothness, self._options)

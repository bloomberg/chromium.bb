# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

from measurements import smoothness
from metrics import power
from telemetry.core import exceptions
from telemetry.core import wpr_modes
from telemetry.page import page
from telemetry.page import page_test
from telemetry.unittest import options_for_unittests
from telemetry.unittest import page_test_test_case

class FakePlatform(object):
  def IsRawDisplayFrameRateSupported(self):
    return False
  def CanMonitorPower(self):
    return False


class FakeBrowser(object):
  def __init__(self):
    self.platform = FakePlatform()
    self.category_filter = None

  def StartTracing(self, category_filter, _):
    self.category_filter = category_filter


class AnimatedPage(page.Page):
  def __init__(self, page_set):
    super(AnimatedPage, self).__init__(
      url='file://animated_page.html',
      page_set=page_set, base_dir=page_set.base_dir)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(.2)


class FakeTab(object):
  def __init__(self):
    self.browser = FakeBrowser()

  def ExecuteJavaScript(self, js):
    pass

class SmoothnessUnitTest(page_test_test_case.PageTestTestCase):
  """Smoke test for smoothness measurement

     Runs smoothness measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """
  def testSyntheticDelayConfiguration(self):
    test_page = page.Page('http://dummy', None)
    test_page.synthetic_delays = {
        'cc.BeginMainFrame': { 'target_duration': 0.012 },
        'cc.DrawAndSwap': { 'target_duration': 0.012, 'mode': 'alternating' },
        'gpu.PresentingFrame': { 'target_duration': 0.012 }
    }

    tab = FakeTab()
    measurement = smoothness.Smoothness()
    measurement.WillStartBrowser(tab.browser)
    measurement.WillNavigateToPage(test_page, tab)
    measurement.WillRunActions(test_page, tab)

    expected_category_filter = [
        'DELAY(cc.BeginMainFrame;0.012000;static)',
        'DELAY(cc.DrawAndSwap;0.012000;alternating)',
        'DELAY(gpu.PresentingFrame;0.012000;static)',
        'benchmark'
    ]
    actual_category_filter = tab.browser.category_filter.split(',')
    actual_category_filter.sort()

    # FIXME: Put blink.console into the expected above and remove these two
    # remove entries when the blink.console change has rolled into chromium.
    actual_category_filter.remove('webkit.console')
    actual_category_filter.remove('blink.console')

    if expected_category_filter != actual_category_filter:
      sys.stderr.write("Expected category filter: %s\n" %
                       repr(expected_category_filter))
      sys.stderr.write("Actual category filter filter: %s\n" %
                       repr(actual_category_filter))
    self.assertEquals(expected_category_filter, actual_category_filter)

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testSmoothness(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    frame_times = results.FindAllPageSpecificValuesNamed('frame_times')
    self.assertEquals(len(frame_times), 1)
    self.assertGreater(frame_times[0].GetRepresentativeNumber(), 0)

    mean_frame_time = results.FindAllPageSpecificValuesNamed('mean_frame_time')
    self.assertEquals(len(mean_frame_time), 1)
    self.assertGreater(mean_frame_time[0].GetRepresentativeNumber(), 0)

    jank = results.FindAllPageSpecificValuesNamed('jank')
    self.assertEquals(len(jank), 1)
    self.assertGreater(jank[0].GetRepresentativeNumber(), 0)

    mostly_smooth = results.FindAllPageSpecificValuesNamed('mostly_smooth')
    self.assertEquals(len(mostly_smooth), 1)
    self.assertGreaterEqual(mostly_smooth[0].GetRepresentativeNumber(), 0)

    mean_input_event_latency = results.FindAllPageSpecificValuesNamed(
        'mean_input_event_latency')
    if mean_input_event_latency:
      self.assertEquals(len(mean_input_event_latency), 1)
      self.assertGreater(
          mean_input_event_latency[0].GetRepresentativeNumber(), 0)

  def testSmoothnessForPageWithNoGesture(self):
    ps = self.CreateEmptyPageSet()
    ps.AddPage(AnimatedPage(ps))

    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    mostly_smooth = results.FindAllPageSpecificValuesNamed('mostly_smooth')
    self.assertEquals(len(mostly_smooth), 1)
    self.assertGreaterEqual(mostly_smooth[0].GetRepresentativeNumber(), 0)

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(smoothness.Smoothness, self._options)

  def testCleanUpPowerMetric(self):
    class FailPage(page.Page):
      def __init__(self, page_set):
        super(FailPage, self).__init__(
            url='file://blank.html',
            page_set=page_set, base_dir=page_set.base_dir)
      def RunSmoothness(self, _):
        raise exceptions.IntentionalException

    class FakePowerMetric(power.PowerMetric):
      start_called = False
      stop_called = True
      def Start(self, _1, _2):
        self.start_called = True
      def Stop(self, _1, _2):
        self.stop_called = True

    ps = self.CreateEmptyPageSet()
    ps.AddPage(FailPage(ps))

    class BuggyMeasurement(smoothness.Smoothness):
      fake_power = None
      # Inject fake power metric.
      def WillStartBrowser(self, browser):
        self.fake_power = self._power_metric = FakePowerMetric(browser)

    measurement = BuggyMeasurement()
    try:
      self.RunMeasurement(measurement, ps)
    except page_test.TestNotSupportedOnPlatformFailure:
      pass

    self.assertTrue(measurement.fake_power.start_called)
    self.assertTrue(measurement.fake_power.stop_called)

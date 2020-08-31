# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import time

from gpu_tests import gpu_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_integration_test_base

from telemetry.util import image_util

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = undefined;
  domAutomationController._finished = false;
  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      // Do not squelch any previous failures. Show any new ones.
      if (domAutomationController._succeeded === undefined ||
          domAutomationController._succeeded)
        domAutomationController._succeeded = (lmsg == "success");
    }
  }

  window.domAutomationController = domAutomationController;
"""


class PixelIntegrationTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
  """GPU pixel tests backed by Skia Gold and Telemetry."""
  test_base_name = 'Pixel'

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.DefaultPages(cls.test_base_name)
    pages += namespace.GpuRasterizationPages(cls.test_base_name)
    pages += namespace.ExperimentalCanvasFeaturesPages(cls.test_base_name)
    pages += namespace.PaintWorkletPages(cls.test_base_name)
    # pages += namespace.NoGpuProcessPages(cls.test_base_name)
    # The following pages should run only on platforms where SwiftShader is
    # enabled. They are skipped on other platforms through test expectations.
    # pages += namespace.SwiftShaderPages(cls.test_base_name)
    if sys.platform.startswith('darwin'):
      pages += namespace.MacSpecificPages(cls.test_base_name)
      # Unfortunately we don't have a browser instance here so can't tell
      # whether we should really run these tests. They're short-circuited to a
      # certain degree on the other platforms.
      pages += namespace.DualGPUMacSpecificPages(cls.test_base_name)
    if sys.platform.startswith('win'):
      pages += namespace.DirectCompositionPages(cls.test_base_name)
      pages += namespace.LowLatencySwapChainPages(cls.test_base_name)
      pages += namespace.HdrTestPages(cls.test_base_name)
    for p in pages:
      yield (p.name, skia_gold_integration_test_base.GPU_RELATIVE_PATH + p.url,
             (p))

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(
        self._AddDefaultArgs(page.browser_args))
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._proceed', timeout=300)
    do_page_action = tab.EvaluateJavaScript(
        'domAutomationController._readyForActions')
    try:
      if do_page_action:
        # The page action may itself signal test failure via self.fail().
        self._DoPageAction(tab, page)
      self._RunSkiaGoldBasedPixelTest(page)
    finally:
      test_messages = _TestHarnessMessages(tab)
      if test_messages:
        logging.info('Logging messages from the test:\n' + test_messages)
      if do_page_action or page.restart_browser_after_test:
        self._RestartBrowser(
            'Must restart after page actions or if required by test')
        if do_page_action and self._IsDualGPUMacLaptop():
          # Give the system a few seconds to reliably indicate that the
          # low-power GPU is active again, to avoid race conditions if the next
          # test makes assertions about the active GPU.
          time.sleep(4)

  def GetExpectedCrashes(self, args):
    """Returns which crashes, per process type, to expect for the current test.

    Args:
      args: The list passed to _RunGpuTest()

    Returns:
      A dictionary mapping crash types as strings to the number of expected
      crashes of that type. Examples include 'gpu' for the GPU process,
      'renderer' for the renderer process, and 'browser' for the browser
      process.
    """
    # args[0] is the PixelTestPage for the current test.
    return args[0].expected_per_process_crashes

  def _RunSkiaGoldBasedPixelTest(self, page):
    """Captures and compares a test image using Skia Gold.

    Raises an Exception if the comparison fails.

    Args:
      page: the GPU PixelTestPage object for the test.
    """
    tab = self.tab
    # Actually run the test and capture the screenshot.
    if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
      self.fail('page indicated test failure')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')
    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    if page.test_rect:
      screenshot = image_util.Crop(screenshot, int(page.test_rect[0] * dpr),
                                   int(page.test_rect[1] * dpr),
                                   int(page.test_rect[2] * dpr),
                                   int(page.test_rect[3] * dpr))

    build_id_args = self._GetBuildIdArgs()

    # Compare images against approved images/colors.
    if page.expected_colors:
      # Use expected colors instead of hash comparison for validation.
      self._ValidateScreenshotSamplesWithSkiaGold(tab, page, screenshot, dpr,
                                                  build_id_args)
      return
    image_name = self._UrlToImageName(page.name)
    self._UploadTestResultToSkiaGold(
        image_name, screenshot, page, build_id_args=build_id_args)

  def _DoPageAction(self, tab, page):
    getattr(self, '_' + page.optional_action)(tab, page)
    # Now that we've done the page's specific action, wait for it to
    # report completion.
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout=300)

  def _AssertLowPowerGPU(self):
    if self._IsDualGPUMacLaptop():
      if not self._IsIntelGPUActive():
        self.fail('Low power GPU should have been active but wasn\'t')

  def _AssertHighPerformanceGPU(self):
    if self._IsDualGPUMacLaptop():
      if self._IsIntelGPUActive():
        self.fail('High performance GPU should have been active but wasn\'t')

  #
  # Optional actions pages can take.
  # These are specified as methods taking the tab and the page as
  # arguments.
  #
  def _CrashGpuProcess(self, tab, page):  # pylint: disable=no-self-use
    # Crash the GPU process.
    #
    # This used to create a new tab and navigate it to
    # chrome://gpucrash, but there was enough unreliability
    # navigating between these tabs (one of which was created solely
    # in order to navigate to chrome://gpucrash) that the simpler
    # solution of provoking the GPU process crash from this renderer
    # process was chosen.
    del page  # Unused in this particular action.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  def _SwitchTabs(self, tab, page):
    del page  # Unused in this particular action.
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    tab.Activate()

  def _RunTestWithHighPerformanceTab(self, tab, page):
    del page  # Unused in this particular action.
    if not self._IsDualGPUMacLaptop():
      # Short-circuit this test.
      logging.info('Short-circuiting test because not running on dual-GPU Mac '
                   'laptop')
      tab.EvaluateJavaScript('initialize(false)')
      tab.action_runner.WaitForJavaScriptCondition(
          'domAutomationController._readyForActions', timeout=30)
      tab.EvaluateJavaScript('runToCompletion()')
      return
    # Reset the ready state of the harness.
    tab.EvaluateJavaScript('domAutomationController._readyForActions = false')
    high_performance_tab = tab.browser.tabs.New()
    high_performance_tab.Navigate(
        self.
        UrlOfStaticFilePath(skia_gold_integration_test_base.GPU_RELATIVE_PATH +
                            'functional_webgl_high_performance.html'),
        script_to_evaluate_on_commit=test_harness_script)
    high_performance_tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout=30)
    # Wait a few seconds for the GPU switched notification to propagate
    # throughout the system.
    time.sleep(5)
    # Switch back to the main tab and quickly start its rendering, while the
    # high-power GPU is still active.
    tab.Activate()
    tab.EvaluateJavaScript('initialize(true)')
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._readyForActions', timeout=30)
    # Close the high-performance tab.
    high_performance_tab.Close()
    # Wait for ~15 seconds for the system to switch back to the
    # integrated GPU.
    time.sleep(15)
    # Run the page to completion.
    tab.EvaluateJavaScript('runToCompletion()')

  def _RunLowToHighPowerTest(self, tab, page):
    del page  # Unused in this particular action.
    is_dual_gpu = self._IsDualGPUMacLaptop()
    tab.EvaluateJavaScript('initialize(' +
                           ('true' if is_dual_gpu else 'false') + ')')
    # The harness above will take care of waiting for the test to
    # complete with either a success or failure.

  def _RunOffscreenCanvasIBRCWebGLTest(self, tab, page):
    del page  # Unused in this particular action.
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('setup()')
    # Wait a few seconds for any (incorrect) GPU switched
    # notifications to propagate throughout the system.
    time.sleep(5)
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('render()')

  def _RunOffscreenCanvasIBRCWebGLHighPerfTest(self, tab, page):
    del page  # Unused in this particular action.
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('setup(true)')
    # Wait a few seconds for any (incorrect) GPU switched
    # notifications to propagate throughout the system.
    time.sleep(5)
    self._AssertHighPerformanceGPU()
    tab.EvaluateJavaScript('render()')

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'pixel_expectations.txt')
    ]


def _TestHarnessMessages(tab):
  return tab.EvaluateJavaScript('domAutomationController._messages')


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

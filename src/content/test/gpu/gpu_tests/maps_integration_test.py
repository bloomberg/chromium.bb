# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

from gpu_tests import common_browser_args as cba
from gpu_tests import color_profile_manager
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import pixel_test_pages
from gpu_tests import expected_color_test

from py_utils import cloud_storage
from telemetry.util import image_util

_MAPS_PERF_TEST_PATH = os.path.join(path_util.GetChromiumSrcDir(), 'tools',
                                    'perf', 'page_sets', 'maps_perf_test')

_DATA_PATH = os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test',
                          'gpu', 'gpu_tests')

_TEST_NAME = 'Maps_maps'


class MapsIntegrationTest(expected_color_test.ExpectedColorTest):
  """Google Maps pixel tests.

  This is an expected color test instead of a regular pixel test because the
  captured image is incredibly noisy.

  Note: this test uses the same WPR as the smoothness.maps benchmark
  in tools/perf/benchmarks. See src/tools/perf/page_sets/maps.py for
  documentation on updating the WPR archive.
  """

  @classmethod
  def Name(cls):
    return 'maps'

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
        options.dont_restore_color_profile_after_test)
    super(MapsIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([
        cba.FORCE_COLOR_PROFILE_SRGB,
        cba.ENSURE_FORCED_COLOR_PROFILE,
    ])
    cloud_storage.GetIfChanged(
        os.path.join(_MAPS_PERF_TEST_PATH, 'load_dataset'),
        cloud_storage.PUBLIC_BUCKET)
    cls.SetStaticServerDirs([_MAPS_PERF_TEST_PATH])
    cls.StartBrowser()

  @classmethod
  def TearDownProcess(cls):
    super(cls, MapsIntegrationTest).TearDownProcess()
    cls.StopWPRServer()

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    # The maps_pixel_expectations.json contain the actual image expectations. If
    # the test fails, with errors greater than the tolerance for the run, then
    # the logs will report the actual failure.
    #
    # There will also be a Skia Gold Triage link, this will be used to store the
    # artifact of the failure to help with debugging. There are no accepted
    # positive baselines recorded in Skia Gold, so its diff will not be
    # sufficient to debugging the failure.
    yield ('Maps_maps', 'file://performance.html', ())

  def RunActualGpuTest(self, url, *_):
    tab = self.tab
    action_runner = tab.action_runner
    action_runner.Navigate(url)
    action_runner.WaitForJavaScriptCondition('window.startTest != undefined')
    action_runner.EvaluateJavaScript('window.startTest()')
    action_runner.WaitForJavaScriptCondition('window.testDone', timeout=320)

    # Wait for the page to process immediate work and load tiles.
    action_runner.EvaluateJavaScript('''
        window.testCompleted = false;
        requestIdleCallback(
            () => window.testCompleted = true,
            { timeout : 10000 })''')
    action_runner.WaitForJavaScriptCondition('window.testCompleted', timeout=30)

    expected = _ReadPixelExpectations('maps_pixel_expectations.json')
    page = _GetMapsPageForUrl(url, expected)

    # Special case some tests on Fuchsia that need to grab the entire contents
    # in the screenshot instead of just the visible portion due to small screen
    # sizes.
    if (MapsIntegrationTest.browser.platform.GetOSName() == 'fuchsia'
        and page.name in pixel_test_pages.PROBLEMATIC_FUCHSIA_TESTS):
      screenshot = tab.FullScreenshot(5)
    else:
      screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    print 'Maps\' devicePixelRatio is ' + str(dpr)

    # The bottom corners of Mac screenshots have black triangles due to the
    # rounded corners of Mac windows. So, crop the bottom few rows off now to
    # get rid of those. The triangles appear to be 5 pixels wide and tall
    # regardless of DPI, so 10 pixels should be sufficient.
    if self.browser.platform.GetOSName() == 'mac':
      img_height, img_width = screenshot.shape[:2]
      screenshot = image_util.Crop(screenshot, 0, 0, img_width, img_height - 10)
    x1, y1, x2, y2 = _GetCropBoundaries(screenshot)
    screenshot = image_util.Crop(screenshot, x1, y1, x2 - x1, y2 - y1)

    self._ValidateScreenshotSamplesWithSkiaGold(tab, page, screenshot, dpr)

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'maps_expectations.txt')
    ]


def _ReadPixelExpectations(expectations_file):
  expectations_path = os.path.join(_DATA_PATH, expectations_file)
  with open(expectations_path, 'r') as f:
    json_contents = json.load(f)
  return json_contents


def _GetMapsPageForUrl(url, expected_colors):
  page = expected_color_test.ExpectedColorPixelTestPage(
      url=url,
      name=_TEST_NAME,
      # Exact test_rect is arbitrary, just needs to encapsulate all pixels
      # that are tested.
      test_rect=[0, 0, 1000, 800],
      tolerance=10,
      expected_colors=expected_colors)
  return page


def _GetCropBoundaries(screenshot):
  """Returns the boundaries to crop the screenshot to.

  Specifically, we look for the boundaries where the white background
  transitions into the (non-white) content we care about.

  Args:
    screenshot: A screenshot returned by Tab.Screenshot() (numpy ndarray?)

  Returns:
    A 4-tuple (x1, y1, x2, y2) denoting the top left and bottom right
    coordinates to crop to.
  """
  img_height, img_width = screenshot.shape[:2]

  def RowIsWhite(row):
    for col in xrange(img_width):
      pixel = image_util.GetPixelColor(screenshot, col, row)
      if pixel.r != 255 or pixel.g != 255 or pixel.b != 255:
        return False
    return True

  def ColumnIsWhite(column):
    for row in xrange(img_height):
      pixel = image_util.GetPixelColor(screenshot, column, row)
      if pixel.r != 255 or pixel.g != 255 or pixel.b != 255:
        return False
    return True

  x1 = y1 = 0
  x2 = img_width
  y2 = img_height
  for column in xrange(img_width):
    if not ColumnIsWhite(column):
      x1 = column
      break

  for row in xrange(img_height):
    if not RowIsWhite(row):
      y1 = row
      break

  for column in xrange(x1 + 1, img_width):
    if ColumnIsWhite(column):
      x2 = column
      break

  for row in xrange(y1 + 1, img_height):
    if RowIsWhite(row):
      y2 = row
      break
  return x1, y1, x2, y2


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

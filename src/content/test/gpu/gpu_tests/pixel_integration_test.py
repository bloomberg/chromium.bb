# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import os
import re
from subprocess import CalledProcessError
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import path_util
from gpu_tests import pixel_test_pages
from gpu_tests import color_profile_manager

from py_utils import cloud_storage
from telemetry.util import image_util

gpu_relative_path = "content/test/data/gpu/"
gpu_data_dir = os.path.join(path_util.GetChromiumSrcDir(), gpu_relative_path)

default_reference_image_dir = os.path.join(gpu_data_dir, 'gpu_reference')

test_data_dirs = [gpu_data_dir,
                  os.path.join(
                      path_util.GetChromiumSrcDir(), 'media/test/data')]

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""


class PixelIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):

  test_base_name = 'Pixel'

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
      options.dont_restore_color_profile_after_test)
    super(PixelIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs(test_data_dirs)

  @staticmethod
  def _AddDefaultArgs(browser_args):
    if not browser_args:
      browser_args = []
    # All tests receive the following options.
    return [
      '--force-color-profile=srgb',
      '--ensure-forced-color-profile',
      '--enable-gpu-benchmarking',
      '--test-type=gpu'] + browser_args

  @classmethod
  def StopBrowser(cls):
    super(PixelIntegrationTest, cls).StopBrowser()
    cls.ResetGpuInfo()

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(PixelIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--reference-dir',
      help='Overrides the default on-disk location for reference images '
      '(only used for local testing without a cloud storage account)',
      default=default_reference_image_dir)
    parser.add_option(
      '--use-skia-gold',
      dest='use_skia_gold',
      action='store_true', default=False,
      help='Use the Skia team\'s Gold tool to handle image comparisons')
    parser.add_option(
      '--no-luci-auth',
      action='store_true', default=False,
      help='Don\'t use the service account provided by LUCI for authentication '
           'for Skia Gold, instead relying on gsutil to be pre-authenticated. '
           'Meant for testing locally instead of on the bots.')

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.DefaultPages(cls.test_base_name)
    pages += namespace.GpuRasterizationPages(cls.test_base_name)
    pages += namespace.ExperimentalCanvasFeaturesPages(cls.test_base_name)
    # pages += namespace.NoGpuProcessPages(cls.test_base_name)
    # The following pages should run only on platforms where SwiftShader is
    # enabled. They are skipped on other platforms through test expectations.
    # pages += namespace.SwiftShaderPages(cls.test_base_name)
    if sys.platform.startswith('darwin'):
      pages += namespace.MacSpecificPages(cls.test_base_name)
    if sys.platform.startswith('win'):
      pages += namespace.DirectCompositionPages(cls.test_base_name)
    for p in pages:
      yield(p.name, gpu_relative_path + p.url, (p))

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(self._AddDefaultArgs(
      page.browser_args))
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._proceed', timeout=300)
    do_page_action = tab.EvaluateJavaScript(
      'domAutomationController._readyForActions')
    if do_page_action:
      self._DoPageAction(tab, page)
    if self.GetParsedCommandLineOptions().use_skia_gold:
      self.RunSkiaGoldBasedPixelTest(test_path, do_page_action, args)
    else:
      self.RunLegacyPixelTest(test_path, do_page_action, args)

  def RunLegacyPixelTest(self, test_path, do_page_action, args):
    page = args[0]
    tab = self.tab
    try:
      if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
        self.fail('page indicated test failure')
      if not tab.screenshot_supported:
        self.fail('Browser does not support screenshot capture')
      screenshot = tab.Screenshot(5)
      if screenshot is None:
        self.fail('Could not capture screenshot')
      dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
      if page.test_rect:
        screenshot = image_util.Crop(
            screenshot, int(page.test_rect[0] * dpr),
            int(page.test_rect[1] * dpr), int(page.test_rect[2] * dpr),
            int(page.test_rect[3] * dpr))
      if page.expected_colors:
        # Use expected colors instead of ref images for validation.
        self._ValidateScreenshotSamples(
            tab, page.name, screenshot, page.expected_colors, page.tolerance,
            dpr)
        return
      image_name = self._UrlToImageName(page.name)
      if self.GetParsedCommandLineOptions().upload_refimg_to_cloud_storage:
        if self._ConditionallyUploadToCloudStorage(image_name, page, tab,
                                                   screenshot):
          # This is the new reference image; there's nothing to compare against.
          ref_png = screenshot
        else:
          # There was a preexisting reference image, so we might as well
          # compare against it.
          ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
      elif self.GetParsedCommandLineOptions().\
          download_refimg_from_cloud_storage:
        # This bot doesn't have the ability to properly generate a
        # reference image, so download it from cloud storage.
        try:
          ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
        except cloud_storage.NotFoundError:
          # There is no reference image yet in cloud storage. This
          # happens when the revision of the test is incremented or when
          # a new test is added, because the trybots are not allowed to
          # produce reference images, only the bots on the main
          # waterfalls. Report this as a failure so the developer has to
          # take action by explicitly suppressing the failure and
          # removing the suppression once the reference images have been
          # generated. Otherwise silent failures could happen for long
          # periods of time.
          self.fail('Could not find image %s in cloud storage' % image_name)
      else:
        # Legacy path using on-disk results.
        ref_png = self._GetReferenceImage(
          self.GetParsedCommandLineOptions().reference_dir,
          image_name, page.revision, screenshot)

      # Test new snapshot against existing reference image
      if not image_util.AreEqual(ref_png, screenshot, tolerance=page.tolerance):
        if self.GetParsedCommandLineOptions().test_machine_name:
          self._UploadErrorImagesToCloudStorage(image_name, screenshot, ref_png)
        else:
          self._WriteErrorImages(
            self.GetParsedCommandLineOptions().generated_dir, image_name,
            screenshot, ref_png)
        self.fail('Reference image did not match captured screen')
    finally:
      if do_page_action:
        # Assume that page actions might have killed the GPU process.
        self._RestartBrowser('Must restart after page actions')

  def RunSkiaGoldBasedPixelTest(self, test_path, do_page_action, args):
    page = args[0]
    tab = self.tab
    try:
      if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
        self.fail('page indicated test failure')
      if not tab.screenshot_supported:
        self.fail('Browser does not support screenshot capture')
      screenshot = tab.Screenshot(5)
      if screenshot is None:
        self.fail('Could not capture screenshot')
      dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
      if page.test_rect:
        screenshot = image_util.Crop(
            screenshot, int(page.test_rect[0] * dpr),
            int(page.test_rect[1] * dpr), int(page.test_rect[2] * dpr),
            int(page.test_rect[3] * dpr))
      # This is required by Gold whether this is a tryjob or not.
      build_id_args = [
        '--commit',
        self.GetParsedCommandLineOptions().build_revision,
      ]
      parsed_options = self.GetParsedCommandLineOptions()
      # Trybots conceptually download their reference images from
      # cloud storage; they don't produce them. (TODO(kbr): remove
      # this once fully switched over to Gold.)
      if parsed_options.download_refimg_from_cloud_storage:
        # Tryjobs in Gold need:
        #   CL issue number (in Gerrit)
        #   Patchset number
        #   Buildbucket ID
        build_id_args += [
          '--issue',
          self.GetParsedCommandLineOptions().review_patch_issue,
          '--patchset',
          self.GetParsedCommandLineOptions().review_patch_set,
          '--jobid',
          self.GetParsedCommandLineOptions().buildbucket_build_id
        ]
      if page.expected_colors:
        # Use expected colors instead of ref images for validation.
        self._ValidateScreenshotSamplesWithSkiaGold(
            tab, page, screenshot, page.expected_colors, page.tolerance,
            dpr, build_id_args)
        return
      image_name = self._UrlToImageName(page.name)
      is_local_run = not (parsed_options.upload_refimg_to_cloud_storage or
                          parsed_options.download_refimg_from_cloud_storage)
      if is_local_run:
        # Legacy path using on-disk results.
        ref_png = self._GetReferenceImage(
          self.GetParsedCommandLineOptions().reference_dir,
          image_name, page.revision, screenshot)
        # Test new snapshot against existing reference image
        if not image_util.AreEqual(ref_png, screenshot,
                                   tolerance=page.tolerance):
          if parsed_options.test_machine_name:
            self._UploadErrorImagesToCloudStorage(image_name, screenshot,
                                                  ref_png)
          else:
            self._WriteErrorImages(
              parsed_options.generated_dir, image_name, screenshot, ref_png)
        self.fail('Reference image did not match captured screen')
      else:
        try:
          self._UploadTestResultToSkiaGold(
            image_name, screenshot,
            tab, page,
            build_id_args=build_id_args)
        except CalledProcessError:
          self.fail('Gold said the test failed, so fail.')
    finally:
      if do_page_action:
        # Assume that page actions might have killed the GPU process.
        self._RestartBrowser('Must restart after page actions')

  def _DoPageAction(self, tab, page):
    getattr(self, '_' + page.optional_action)(tab, page)
    # Now that we've done the page's specific action, wait for it to
    # report completion.
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout=300)

  def _DeleteOldReferenceImages(self, ref_image_path, cur_revision):
    if not cur_revision:
      return

    old_revisions = glob.glob(ref_image_path + "_*.png")
    for rev_path in old_revisions:
      m = re.match(r'^.*_(\d+)\.png$', rev_path)
      if m and int(m.group(1)) < cur_revision:
        print 'Found deprecated reference image. Deleting rev ' + m.group(1)
        os.remove(rev_path)

  def _GetReferenceImage(self, img_dir, img_name, cur_revision, screenshot):
    if not cur_revision:
      cur_revision = 0

    image_path = os.path.join(img_dir, img_name)

    self._DeleteOldReferenceImages(image_path, cur_revision)

    image_path = image_path + '_v' + str(cur_revision) + '.png'

    try:
      ref_png = image_util.FromPngFile(image_path)
    # This can raise a couple of exceptions including IOError and ValueError.
    except Exception:
      ref_png = None

    if ref_png is not None:
      return ref_png

    print ('Reference image not found. Writing tab contents as reference to: ' +
           image_path)

    self._WriteImage(image_path, screenshot)
    return screenshot

  #
  # Optional actions pages can take.
  # These are specified as methods taking the tab and the page as
  # arguments.
  #
  def _CrashGpuProcess(self, tab, page):
    # Crash the GPU process.
    #
    # This used to create a new tab and navigate it to
    # chrome://gpucrash, but there was enough unreliability
    # navigating between these tabs (one of which was created solely
    # in order to navigate to chrome://gpucrash) that the simpler
    # solution of provoking the GPU process crash from this renderer
    # process was chosen.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations',
                     'pixel_expectations.txt')]

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Web Platform Tests (WPT) on Android browsers.

This script supports running tests on the Chromium Waterfall by mapping isolated
script flags to WPT flags.

It is also useful for local reproduction by performing APK installation and
configuring the browser to resolve test hosts.  Be sure to invoke this
executable directly rather than using python run_android_wpt.py so that
WPT dependencies in Chromium vpython are found.

If you need more advanced test control, please use the runner located at
//third_party/blink/web_tests/external/wpt/wpt.

Here's the mapping [isolate script flag] : [wpt flag]
--isolated-script-test-output : --log-chromium
--total-shards : --total-chunks
--shard-index : -- this-chunk
"""

# TODO(aluo): Combine or factor out commons parts with run_wpt_tests.py script.

import argparse
import contextlib
import json
import logging
import os
import shutil
import sys

import common

logger = logging.getLogger(__name__)

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

import devil_chromium

from devil import devil_env
from devil.android import apk_helper
from devil.android import device_utils
from devil.android.tools import system_app
from devil.android.tools import webview_app

CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')

if PYUTILS not in sys.path:
  sys.path.append(PYUTILS)

from py_utils.tempfile_ext import NamedTemporaryDirectory

BLINK_TOOLS_DIR = os.path.join(SRC_DIR, 'third_party', 'blink', 'tools')
WEB_TESTS_DIR = os.path.join(BLINK_TOOLS_DIR, os.pardir, 'web_tests')
DEFAULT_WPT = os.path.join(WEB_TESTS_DIR, 'external', 'wpt', 'wpt')

ANDROID_WEBLAYER = 'android_weblayer'
ANDROID_WEBVIEW = 'android_webview'
CHROME_ANDROID = 'chrome_android'

# List of supported products.
PRODUCTS = [ANDROID_WEBLAYER, ANDROID_WEBVIEW, CHROME_ANDROID]


class PassThroughArgs(argparse.Action):
  pass_through_args = []
  def __call__(self, parser, namespace, values, option_string=None):
    if option_string:
      if self.nargs == 0:
        self.add_unique_pass_through_arg(option_string)
      elif self.nargs is None:
        self.add_unique_pass_through_arg('{}={}'.format(option_string, values))
      else:
        raise ValueError("nargs {} not supported: {} {}".format(
            self.nargs, option_string, values))

  @classmethod
  def add_unique_pass_through_arg(cls, arg):
    if arg not in cls.pass_through_args:
      cls.pass_through_args.append(arg)


def _get_adapter(device):
  usage = '%(prog)s --product={' + ','.join(PRODUCTS) + '} ...'
  product_parser = argparse.ArgumentParser(
      add_help=False, prog='run_android_wpt.py', usage=usage)
  product_parser.add_argument(
      '--product', action='store', required=True, choices=PRODUCTS)
  options, _ = product_parser.parse_known_args()
  product = options.product

  if product == ANDROID_WEBLAYER:
    return WPTWeblayerAdapter(device)
  elif product == ANDROID_WEBVIEW:
    return WPTWebviewAdapter(device)
  else:
    return WPTClankAdapter(device)


class WPTAndroidAdapter(common.BaseIsolatedScriptArgsAdapter):

  def __init__(self, device):
    self.pass_through_wpt_args = []
    self.pass_through_binary_args = []
    self._metadata_dir = None
    self._test_apk = None
    self._missing_test_apk_arg = None
    self._device = device
    super(WPTAndroidAdapter, self).__init__()
    # Arguments from add_extra_argumentsparse were added so
    # its safe to parse the arguments and set self._options
    self.parse_args()

  def generate_test_output_args(self, output):
    return ['--log-chromium', output]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-chunks=%d' % total_shards,
        # shard_index is 0-based but WPT's this-chunk to be 1-based
        '--this-chunk=%d' % (shard_index + 1)]

  @property
  def rest_args(self):
    rest_args = super(WPTAndroidAdapter, self).rest_args

    # Here we add all of the arguments required to run WPT tests on Android.
    rest_args.extend([self.options.wpt_path])

    # vpython has packages needed by wpt, so force it to skip the setup
    rest_args.extend(["--venv=../../", "--skip-venv-setup"])

    rest_args.extend(["run",
      "--test-type=" + self.options.test_type,
      "--webdriver-binary",
      self.options.webdriver_binary,
      "--headless",
      "--no-pause-after-test",
      "--no-capture-stdio",
      "--no-manifest-download",
    ])
    # if metadata was created then add the metadata directory
    # to the list of wpt arguments
    if self._metadata_dir:
      rest_args.extend(['--metadata', self._metadata_dir])

    if self.options.verbose >= 3:
      rest_args.extend(["--log-mach=-", "--log-mach-level=debug",
                        "--log-mach-verbose"])

    if self.options.verbose >= 4:
      rest_args.extend(['--webdriver-arg=--verbose',
                        '--webdriver-arg="--log-path=-"'])

    rest_args.extend(self.pass_through_wpt_args)

    return rest_args

  def _extra_metadata_builder_args(self):
    raise NotImplementedError

  def _maybe_build_metadata(self):
    if not self._test_apk:
      assert self._missing_test_apk_arg, (
          'self._missing_test_apk_arg was not set.')
      logger.warning('%s was not set, skipping metadata generation.' %
                     self._missing_test_apk_arg)
      return

    metadata_builder_cmd = [
         sys.executable,
         os.path.join(BLINK_TOOLS_DIR, 'build_wpt_metadata.py'),
         '--android-apk',
         self._test_apk,
         '--metadata-output-dir',
         self._metadata_dir,
         '--additional-expectations',
         os.path.join(WEB_TESTS_DIR, 'android', 'AndroidWPTNeverFixTests')
    ]
    metadata_builder_cmd.extend(self._extra_metadata_builder_args())
    common.run_command(metadata_builder_cmd)

  def run_test(self):
    with NamedTemporaryDirectory() as self._metadata_dir, self._install_apks():
      self._maybe_build_metadata()
      return super(WPTAndroidAdapter, self).run_test()

  def _install_apks(self):
    raise NotImplementedError

  def clean_up_after_test_run(self):
    # Avoid having a dangling reference to the temp directory
    # which was deleted
    self._metadata_dir = None

  def do_post_test_run_tasks(self):
    # Move json results into layout-test-results directory
    results_dir = os.path.dirname(self.options.isolated_script_test_output)
    layout_test_results = os.path.join(results_dir, 'layout-test-results')
    os.mkdir(layout_test_results)
    shutil.copyfile(self.options.isolated_script_test_output,
                    os.path.join(layout_test_results, 'full_results.json'))
    # create full_results_jsonp.js file which is used to
    # load results into the results viewer
    with open(self.options.isolated_script_test_output, 'r') as full_results, \
         open(os.path.join(
             layout_test_results, 'full_results_jsonp.js'), 'w') as json_js:
      json_js.write('ADD_FULL_RESULTS(%s);' % full_results.read())
    # copy layout test results viewer to layout-test-results directory
    shutil.copyfile(
        os.path.join(WEB_TESTS_DIR, 'fast', 'harness', 'results.html'),
        os.path.join(layout_test_results, 'results.html'))

  def add_extra_arguments(self, parser):
    # TODO: |pass_through_args| are broke and need to be supplied by way of
    # --binary-arg".
    class BinaryPassThroughArgs(PassThroughArgs):
      pass_through_args = self.pass_through_binary_args
    class WPTPassThroughArgs(PassThroughArgs):
      pass_through_args = self.pass_through_wpt_args

    # Add this so that product argument does not go in self._rest_args
    # when self.parse_args() is called
    parser.add_argument('--product', help=argparse.SUPPRESS)
    parser.add_argument('--webdriver-binary', required=True,
                        help='Path of the webdriver binary.  It needs to have'
                        ' the same major version as the apk.')
    parser.add_argument('--wpt-path', default=DEFAULT_WPT,
                        help='Controls the path of the WPT runner to use'
                        ' (therefore tests).  Defaults the revision rolled into'
                        ' Chromium.')
    parser.add_argument('--test-type', default='testharness',
                        help='Specify to experiment with other test types.'
                        ' Currently only the default is expected to work.')
    parser.add_argument('--verbose', '-v', action='count',
                        help='Verbosity level.')
    parser.add_argument('--include', metavar='TEST_OR_DIR',
                        action=WPTPassThroughArgs,
                        help='Test(s) to run, defaults to run all tests.')
    parser.add_argument('--list-tests', action=WPTPassThroughArgs, nargs=0,
                        help="Don't run any tests, just print out a list of"
                        ' tests that would be run.')
    parser.add_argument('--webdriver-arg', action=WPTPassThroughArgs,
                        help='WebDriver args.')
    parser.add_argument('--log-wptreport', metavar='WPT_REPORT_FILE',
                        action=WPTPassThroughArgs,
                        help="Log wptreport with subtest details.")
    parser.add_argument('--log-raw', metavar='RAW_REPORT_FILE',
                        action=WPTPassThroughArgs,
                        help="Log raw report.")
    parser.add_argument('--log-html', metavar='HTML_REPORT_FILE',
                        action=WPTPassThroughArgs,
                        help="Log html report.")
    parser.add_argument('--log-xunit', metavar='XUNIT_REPORT_FILE',
                        action=WPTPassThroughArgs,
                        help="Log xunit report.")
    parser.add_argument('--enable-features', action=BinaryPassThroughArgs,
                        help='Chromium features to enable during testing.')
    parser.add_argument('--disable-features', action=BinaryPassThroughArgs,
                        help='Chromium features to disable during testing.')
    parser.add_argument('--disable-field-trial-config',
                        action=BinaryPassThroughArgs,
                        help='Disable test trials for Chromium features.')
    parser.add_argument('--force-fieldtrials', action=BinaryPassThroughArgs,
                        help='Force trials for Chromium features.')
    parser.add_argument('--force-fieldtrial-params',
                        action=BinaryPassThroughArgs,
                        help='Force trial params for Chromium features.')


class WPTWeblayerAdapter(WPTAndroidAdapter):

  WEBLAYER_SHELL_PKG = 'org.chromium.weblayer.shell'
  WEBLAYER_SUPPORT_PKG = 'org.chromium.weblayer.support'

  def __init__(self, device):
    super(WPTWeblayerAdapter, self).__init__(device)
    self._test_apk = self.options.weblayer_shell
    self._missing_test_apk_arg = '--weblayer-shell'

  @contextlib.contextmanager
  def _install_apks(self):
    install_weblayer_shell_as_needed = maybe_install_user_apk(
        self._device, self.options.weblayer_shell, self.WEBLAYER_SHELL_PKG)
    install_weblayer_support_as_needed = maybe_install_user_apk(
        self._device, self.options.weblayer_support, self.WEBLAYER_SUPPORT_PKG)
    install_webview_provider_as_needed = maybe_install_webview_provider(
        self._device, self.options.webview_provider)

    with install_weblayer_shell_as_needed,   \
         install_weblayer_support_as_needed, \
         install_webview_provider_as_needed:
      yield

  def _extra_metadata_builder_args(self):
    return [
      '--additional-expectations',
      os.path.join(WEB_TESTS_DIR,
                   'android', 'WeblayerWPTOverrideExpectations')]

  def add_extra_arguments(self, parser):
    super(WPTWeblayerAdapter, self).add_extra_arguments(parser)
    parser.add_argument('--weblayer-shell',
                        help='WebLayer Shell apk to install.')
    parser.add_argument('--weblayer-support',
                        help='WebLayer Support apk to install.')
    parser.add_argument('--webview-provider',
                        help='Webview provider apk to install.')

  @property
  def rest_args(self):
    args = super(WPTWeblayerAdapter, self).rest_args
    args.append(ANDROID_WEBLAYER)
    return args


class WPTWebviewAdapter(WPTAndroidAdapter):

  SYSTEM_WEBVIEW_SHELL_PKG = 'org.chromium.webview_shell'

  def __init__(self, device):
    super(WPTWebviewAdapter, self).__init__(device)
    self._test_apk = self.options.system_webview_shell
    self._missing_test_apk_arg = '--system-webview-shell'

  @contextlib.contextmanager
  def _install_apks(self):
    install_shell_as_needed = maybe_install_user_apk(
        self._device, self.options.system_webview_shell,
        self.SYSTEM_WEBVIEW_SHELL_PKG)
    install_webview_provider_as_needed = maybe_install_webview_provider(
        self._device, self.options.webview_provider)

    with install_shell_as_needed, install_webview_provider_as_needed:
      yield

  def _extra_metadata_builder_args(self):
    return [
      '--additional-expectations',
      os.path.join(
          WEB_TESTS_DIR, 'android', 'WebviewWPTOverrideExpectations')]

  def add_extra_arguments(self, parser):
    super(WPTWebviewAdapter, self).add_extra_arguments(parser)
    parser.add_argument('--system-webview-shell',
                        help=('System WebView Shell apk to install. If not '
                              'specified then the on-device WebView apk '
                              'will be used.'))
    parser.add_argument('--webview-provider',
                        help='Webview provider APK to install.')

  @property
  def rest_args(self):
    args = super(WPTWebviewAdapter, self).rest_args
    args.append(ANDROID_WEBVIEW)
    return args

class WPTClankAdapter(WPTAndroidAdapter):

  def __init__(self, device):
    super(WPTClankAdapter, self).__init__(device)
    self._test_apk = self.options.chrome_apk
    self._missing_test_apk_arg = '--chrome-apk'

  @contextlib.contextmanager
  def _install_apks(self):
    install_clank_as_needed = maybe_install_user_apk(
        self._device, self.options.chrome_apk)
    with install_clank_as_needed:
      yield

  def _extra_metadata_builder_args(self):
    return [
      '--additional-expectations',
      os.path.join(WEB_TESTS_DIR, 'android', 'ClankWPTOverrideExpectations')]

  def add_extra_arguments(self, parser):
    super(WPTClankAdapter, self).add_extra_arguments(parser)
    parser.add_argument(
        '--chrome-apk', help='Chrome apk to install.')
    parser.add_argument(
        '--chrome-package-name',
        help=('The package name of Chrome to test,'
              ' defaults to that of the compiled Chrome apk.'))

  @property
  def rest_args(self):
    args = super(WPTClankAdapter, self).rest_args
    if not self.options.chrome_package_name and not self.options.chrome_apk:
      raise Exception('Either the --chrome-package-name or --chrome-apk '
                      'command line arguments must be used.')
    if not self.options.chrome_package_name:
      self.options.chrome_package_name = apk_helper.GetPackageName(
          self.options.chrome_apk)
      logger.info("Using Chrome apk's default package %s." %
                  self.options.chrome_package_name)
    args.extend(['--package-name', self.options.chrome_package_name])
    # add the product postional argument
    args.append(CHROME_ANDROID)
    return args


def maybe_install_webview_provider(device, apk):
  if apk:
    logger.info('Will install WebView apk at ' + apk)
    return webview_app.UseWebViewProvider(device, apk)
  else:
    return no_op()


def maybe_install_user_apk(device, apk, expected_pkg=None):
  """contextmanager to install apk on device.

  Args:
    device: DeviceUtils instance on which to install the apk.
    apk: Apk file path on host.
    expected_pkg:  Optional, check that apk's package name matches.
  Returns:
    If apk evaluates to false, returns a do-nothing contextmanager.
    Otherwise, returns a contextmanager to install apk on device.
  """
  if apk:
    pkg = apk_helper.GetPackageName(apk)
    if expected_pkg and pkg != expected_pkg:
      raise ValueError('{} has incorrect package name: {}, expected {}.'.format(
          apk, pkg, expected_pkg))
    install_as_needed = app_installed(device, apk)
    logger.info('Will install ' + pkg + ' at ' + apk)
  else:
    install_as_needed = no_op()
  return install_as_needed


@contextlib.contextmanager
def app_installed(device, apk):
  pkg = apk_helper.GetPackageName(apk)
  device.Install(apk)
  try:
    yield
  finally:
    device.Uninstall(pkg)


# Dummy contextmanager to simplify multiple optional managers.
@contextlib.contextmanager
def no_op():
  yield


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


def main():
  devil_chromium.Initialize()
  devices = device_utils.DeviceUtils.HealthyDevices()

  if not devices:
    logger.error('There are no devices attached to this host. Exiting script.')
    return 1

  # Only 1 device is supported for Android locally, this will work well with
  # sharding support via swarming infra.
  device = devices[0]

  adapter = _get_adapter(device)

  if adapter.options.verbose:
    if adapter.options.verbose == 1:
      logger.setLevel(logging.INFO)
    else:
      logger.setLevel(logging.DEBUG)

  # WPT setup for chrome and webview requires that PATH contains adb.
  platform_tools_path = os.path.dirname(devil_env.config.FetchPath('adb'))
  os.environ['PATH'] = ':'.join([platform_tools_path] +
                                os.environ['PATH'].split(':'))

  return adapter.run_test()


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    logging.basicConfig(level=logging.WARNING)
    logger = logging.getLogger()
    sys.exit(main())

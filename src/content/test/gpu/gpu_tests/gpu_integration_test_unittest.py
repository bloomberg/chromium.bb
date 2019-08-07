# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest
import mock
import sys
import run_gpu_integration_test
import gpu_project_config


from telemetry.testing import browser_test_runner
from telemetry.internal.platform import system_info

from gpu_tests import context_lost_integration_test
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import webgl_conformance_integration_test

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'tools', 'perf')
from chrome_telemetry_build import chromium_config

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

VENDOR_STRING_IMAGINATION = 'Imagination Technologies'
DEVICE_STRING_SGX = 'PowerVR SGX 554'

class MockPlatform(object):
  def __init__(self, os_name, os_version_name=None):
    self.os_name = os_name
    self.os_version_name = os_version_name

  def GetOSName(self):
    return self.os_name

  def GetOSVersionName(self):
    return self.os_version_name


class MockBrowser(object):
  def __init__(self, platform, gpu='', device='', vendor_string='',
               device_string='', browser_type=None, gl_renderer=None,
               passthrough=False):
    self.platform = platform
    self.browser_type = browser_type
    sys_info = {
      'model_name': '',
      'gpu': {
        'devices': [
          {'vendor_id': gpu, 'device_id': device,
           'vendor_string': vendor_string, 'device_string': device_string},
        ],
       'aux_attributes': {'passthrough_cmd_decoder': passthrough}
      }
    }
    if gl_renderer:
      sys_info['gpu']['aux_attributes']['gl_renderer'] = gl_renderer
    self.system_info = system_info.SystemInfo.FromDict(sys_info)

  def GetSystemInfo(self):
    return self.system_info

  def __enter__(self):
    return self

  def __exit__(self, *args):
    pass


class MockArgs(object):

  def __init__(self, is_asan=False, webgl_version='1.0.0'):
    self.is_asan = is_asan
    self.webgl_conformance_version = webgl_version
    self.webgl2_only = False
    self.browser_options = []


class MockAbstractGpuTestClass(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def GenerateGpuTests(cls, options):
    pass

  def RunActualGpuTest(self, test_path, *args):
    pass

  @classmethod
  def _CreateExpectations(cls):
    pass


class MockTestCaseWithoutExpectationsFile(MockAbstractGpuTestClass):
  pass


class MockTestCaseWithExpectationsFile(MockAbstractGpuTestClass):

  @classmethod
  def ExpectationsFiles(cls):
    return ['example_test_expectations.txt']


class MockPossibleBrowser(object):

  def __init__(self, browser=None):
    self._returned_browser = browser

  def BrowserSession(self, options):
    del options
    return self._returned_browser


def _generateNvidiaExampleTagsForTestClassAndArgs(test_class, args):
  class MockTestCase(test_class, MockAbstractGpuTestClass):

    @classmethod
    def ExpectationsFiles(cls):
      return ['example_test_expectations.txt']

  _ = [_ for _ in MockTestCase.GenerateGpuTests(args)]
  platform = MockPlatform('win', 'win10')
  browser = MockBrowser(
      platform, VENDOR_NVIDIA, 0x1cb3, browser_type='release',
      gl_renderer='ANGLE Direct3D9')
  possible_browser = MockPossibleBrowser(browser)
  return set(MockTestCase.GenerateTags(args, possible_browser))


class GpuIntegrationTestUnittest(unittest.TestCase):
  def setUp(self):
    self._test_state = {}
    self._test_result = {}

  def _RunGpuIntegrationTests(self, test_name, extra_args=None):
    extra_args = extra_args or []
    temp_file = tempfile.NamedTemporaryFile(delete=False)
    temp_file.close()
    try:
      sys.argv = [
          run_gpu_integration_test.__file__,
          test_name,
          '--write-full-results-to=%s' % temp_file.name,
          ] + extra_args
      gpu_project_config.CONFIG = chromium_config.ChromiumConfig(
          top_level_dir=path_util.GetGpuTestDir(),
          benchmark_dirs=[
              os.path.join(path_util.GetGpuTestDir(), 'unittest_data')])
      run_gpu_integration_test.main()
      with open(temp_file.name) as f:
        self._test_result = json.load(f)
    finally:
      temp_file.close()

  def testOverrideDefaultRetryArgumentsinRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests(
        'run_tests_with_expectations_files', ['--retry-limit=1'])
    self.assertEqual(
        self._test_result['tests']['a']['b']
        ['unexpected-fail.html']['actual'],
        'FAIL FAIL')

  def testDefaultRetryArgumentsinRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests('run_tests_with_expectations_files')
    self.assertEqual(
        self._test_result['tests']['a']['b']['expected-flaky.html']['actual'],
        'FAIL FAIL FAIL')

  def testTestNamePrefixGenerationInRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests('simple_integration_unittest')
    self.assertIn('expected_failure', self._test_result['tests'])

  def testWithoutExpectationsFilesGenerateTagsReturnsEmptyList(self):
    # we need to make sure that GenerateTags() returns an empty list if
    # there are no expectations files returned from ExpectationsFiles() or
    # else Typ will raise an exception
    args = MockArgs()
    possible_browser = MockPossibleBrowser()
    self.assertFalse(MockTestCaseWithoutExpectationsFile.GenerateTags(
        args, possible_browser))

  def _TestTagGenerationForMockPlatform(self, test_class, args):
    tag_set = _generateNvidiaExampleTagsForTestClassAndArgs(
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest,
        args)
    self.assertTrue(
        set(['win', 'win10', 'd3d9', 'release',
             'nvidia', 'nvidia-0x1cb3', 'no-passthrough']).issubset(tag_set))
    return tag_set

  def testGenerateContextLostExampleTagsForAsan(self):
    args = MockArgs(is_asan=True)
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args)
    self.assertIn('asan', tag_set)
    self.assertNotIn('no-asan', tag_set)

  def testGenerateContextLostExampleTagsForNoAsan(self):
    args = MockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args)
    self.assertIn('no-asan', tag_set)
    self.assertNotIn('asan', tag_set)

  def testGenerateWebglConformanceExampleTagsForWebglVersion1andAsan(self):
    args = MockArgs(is_asan=True, webgl_version='1.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest,
        args)
    self.assertTrue(set(['asan', 'webgl-version-1']).issubset(tag_set))
    self.assertFalse(set(['no-asan', 'webgl-version-2']) & tag_set)

  def testGenerateWebglConformanceExampleTagsForWebglVersion2andNoAsan(self):
    args = MockArgs(is_asan=False, webgl_version='2.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest,
        args)
    self.assertTrue(set(['no-asan', 'webgl-version-2']) .issubset(tag_set))
    self.assertFalse(set(['asan', 'webgl-version-1']) & tag_set)

  def testGenerateNvidiaExampleTags(self):
    args = MockArgs()
    platform = MockPlatform('win', 'win10')
    browser = MockBrowser(
        platform, VENDOR_NVIDIA, 0x1cb3, browser_type='release',
        gl_renderer='ANGLE Direct3D9')
    possible_browser = MockPossibleBrowser(browser)
    self.assertEqual(
        set(MockTestCaseWithExpectationsFile.GenerateTags(
            args, possible_browser)),
        set(['win', 'win10', 'release', 'nvidia', 'nvidia-0x1cb3',
             'd3d9', 'no-passthrough']))

  def testGenerateVendorTagUsingVendorString(self):
    args = MockArgs()
    platform = MockPlatform('mac', 'mojave')
    browser = MockBrowser(
        platform, browser_type='release',
        gl_renderer='ANGLE OpenGL ES', passthrough=True,
        vendor_string=VENDOR_STRING_IMAGINATION,
        device_string=DEVICE_STRING_SGX)
    possible_browser = MockPossibleBrowser(browser)
    self.assertEqual(
        set(MockTestCaseWithExpectationsFile.GenerateTags(
            args, possible_browser)),
        set(['mac', 'mojave', 'release', 'imagination',
             'imagination-powervr-sgx-554',
             'opengles', 'passthrough']))

  def testGenerateVendorTagUsingDeviceString(self):
    args = MockArgs()
    platform = MockPlatform('mac', 'mojave')
    browser = MockBrowser(
        platform, browser_type='release',
        vendor_string='illegal vendor string',
        device_string='ANGLE (Imagination, Triangle Monster 3000, 1.0)')
    possible_browser = MockPossibleBrowser(browser)
    self.assertEqual(
        set(MockTestCaseWithExpectationsFile.GenerateTags(
            args, possible_browser)),
        set(['mac', 'mojave', 'release', 'imagination',
             'imagination-triangle-monster-3000',
             'no-angle', 'no-passthrough']))

  def testSimpleIntegrationTest(self):
    self._RunIntegrationTest(
      'simple_integration_unittest',
      ['unittest_data.integration_tests.SimpleTest.unexpected_error',
       'unittest_data.integration_tests.SimpleTest.unexpected_failure'],
      ['unittest_data.integration_tests.SimpleTest.expected_flaky',
       'unittest_data.integration_tests.SimpleTest.expected_failure'],
      ['unittest_data.integration_tests.SimpleTest.expected_skip'],
      [])
    # It might be nice to be more precise about the order of operations
    # with these browser restarts, but this is at least a start.
    self.assertEquals(self._test_state['num_browser_starts'], 6)

  def testIntegrationTesttWithBrowserFailure(self):
    self._RunIntegrationTest(
      'browser_start_failure_integration_unittest', [],
      ['unittest_data.integration_tests.BrowserStartFailureTest.restart'],
      [], [])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testIntegrationTestWithBrowserCrashUponStart(self):
    self._RunIntegrationTest(
      'browser_crash_after_start_integration_unittest', [],
      [('unittest_data.integration_tests.BrowserCrashAfterStartTest.restart')],
      [], [])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testRetryLimit(self):
    self._RunIntegrationTest(
      'test_retry_limit',
      ['unittest_data.integration_tests.TestRetryLimit.unexpected_failure'],
      [],
      [],
      ['--retry-limit=2'])
    # The number of attempted runs is 1 + the retry limit.
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def _RunTestsWithExpectationsFiles(self):
    self._RunIntegrationTest(
      'run_tests_with_expectations_files',
      [('a/b/unexpected-fail.html')],
      [('a/b/expected-fail.html'),
       ('a/b/expected-flaky.html')],
      [('a/b/expected-skip.html')],
      ['--retry-limit=3', '--retry-only-retry-on-failure-tests',
       ('--test-name-prefix=unittest_data.integration_tests.'
        'RunTestsWithExpectationsFiles.')])

  def testUseTestExpectationsFileToHandleExpectedSkip(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-skip.html']
    self.assertEqual(results['expected'], 'SKIP')
    self.assertEqual(results['actual'], 'SKIP')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleUnexpectedTestFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['unexpected-fail.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-fail.html']
    self.assertEqual(results['expected'], 'FAIL')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFlakyTest(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-flaky.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL FAIL FAIL PASS')
    self.assertNotIn('is_regression', results)

  def testRepeat(self):
    self._RunIntegrationTest(
      'test_repeat',
      [],
      ['unittest_data.integration_tests.TestRepeat.success'],
      [],
      ['--repeat=3'])
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def testAlsoRunDisabledTests(self):
    self._RunIntegrationTest(
      'test_also_run_disabled_tests',
      ['unittest_data.integration_tests.TestAlsoRunDisabledTests.skip',
       'unittest_data.integration_tests.TestAlsoRunDisabledTests.flaky'],
      # Tests that are expected to fail and do fail are treated as test passes
      [('unittest_data.integration_tests.'
        'TestAlsoRunDisabledTests.expected_failure')],
      [],
      ['--also-run-disabled-tests'])
    self.assertEquals(self._test_state['num_flaky_test_runs'], 4)
    self.assertEquals(self._test_state['num_test_runs'], 6)

  def testStartBrowser_Retries(self):
    class TestException(Exception):
      pass
    def SetBrowserAndRaiseTestException():
      gpu_integration_test.GpuIntegrationTest.browser = (
          mock.MagicMock())
      raise TestException
    gpu_integration_test.GpuIntegrationTest.browser = None
    gpu_integration_test.GpuIntegrationTest.platform = None
    with mock.patch.object(
        gpu_integration_test.serially_executed_browser_test_case.\
            SeriallyExecutedBrowserTestCase,
            'StartBrowser',
            side_effect=SetBrowserAndRaiseTestException) as mock_start_browser:
      with mock.patch.object(
          gpu_integration_test.GpuIntegrationTest,
          'StopBrowser') as mock_stop_browser:
        with self.assertRaises(TestException):
          gpu_integration_test.GpuIntegrationTest.StartBrowser()
        self.assertEqual(mock_start_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)
        self.assertEqual(mock_stop_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)

  def _RunIntegrationTest(self, test_name, failures, successes, skips,
                          additional_args):
    config = chromium_config.ChromiumConfig(
        top_level_dir=path_util.GetGpuTestDir(),
        benchmark_dirs=[
            os.path.join(path_util.GetGpuTestDir(), 'unittest_data')])
    temp_dir = tempfile.mkdtemp()
    test_results_path = os.path.join(temp_dir, 'test_results.json')
    test_state_path = os.path.join(temp_dir, 'test_state.json')
    try:
      browser_test_runner.Run(
          config,
          [test_name,
           '--write-full-results-to=%s' % test_results_path,
           '--test-state-json-path=%s' % test_state_path] + additional_args)
      with open(test_results_path) as f:
        self._test_result = json.load(f)
      with open(test_state_path) as f:
        self._test_state = json.load(f)
      actual_successes, actual_failures, actual_skips = (
          self._ExtractTestResults(self._test_result))
      self.assertEquals(set(actual_failures), set(failures))
      self.assertEquals(set(actual_successes), set(successes))
      self.assertEquals(set(actual_skips), set(skips))
    finally:
      shutil.rmtree(temp_dir)

  def _ExtractTestResults(self, test_result):
    delimiter = test_result['path_delimiter']
    failures = []
    successes = []
    skips = []
    def _IsLeafNode(node):
      test_dict = node[1]
      return ('expected' in test_dict and
              isinstance(test_dict['expected'], basestring))
    node_queues = []
    for t in test_result['tests']:
      node_queues.append((t, test_result['tests'][t]))
    while node_queues:
      node = node_queues.pop()
      full_test_name, test_dict = node
      if _IsLeafNode(node):
        if all(res not in test_dict['expected'].split() for res in
               test_dict['actual'].split()):
          failures.append(full_test_name)
        elif test_dict['expected'] == test_dict['actual'] == 'SKIP':
          skips.append(full_test_name)
        else:
          successes.append(full_test_name)
      else:
        for k in test_dict:
          node_queues.append(
            ('%s%s%s' % (full_test_name, delimiter, k),
             test_dict[k]))
    return successes, failures, skips

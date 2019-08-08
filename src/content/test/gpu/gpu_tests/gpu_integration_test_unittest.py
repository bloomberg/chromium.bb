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
import inspect
import itertools

from collections import defaultdict

from telemetry.testing import browser_test_runner
from telemetry.internal.platform import system_info

from gpu_tests import context_lost_integration_test
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import pixel_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import webgl_conformance_integration_test
from gpu_tests import webgl_test_util

from py_utils import discover
from typ import expectations_parser
from typ import json_results

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'tools', 'perf')
from chrome_telemetry_build import chromium_config

OS_CONDITIONS = ['win', 'mac', 'android']
GPU_CONDITIONS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                  'nvidia', 'qualcomm', 'vivante']
WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = ['leopard', 'snowleopard', 'lion', 'mountainlion',
                  'mavericks', 'yosemite', 'sierra', 'highsierra', 'mojave']
# These aren't expanded out into "lollipop", "marshmallow", etc.
ANDROID_CONDITIONS = ['l', 'm', 'n', 'o', 'p', 'q']
GENERIC_CONDITIONS = OS_CONDITIONS + GPU_CONDITIONS

_map_specific_to_generic = {sos:'win' for sos in WIN_CONDITIONS}
_map_specific_to_generic.update({sos:'mac' for sos in MAC_CONDITIONS})
_map_specific_to_generic.update({sos:'android' for sos in ANDROID_CONDITIONS})

_get_generic = lambda tags: set(
    [_map_specific_to_generic.get(tag, tag) for tag in tags])

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

VENDOR_STRING_IMAGINATION = 'Imagination Technologies'
DEVICE_STRING_SGX = 'PowerVR SGX 554'

ResultType = json_results.ResultType

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
    self.url = 'https://www.google.com'
    self.duration = 10
    self.delay = 10
    self.resolution = 100
    self.fullscreen = False
    self.underlay = False
    self.logdir = '/tmp'
    self.repeat = 1
    self.outliers = 0
    self.bypass_ipg = False
    self.expected_vendor_id = 0
    self.expected_device_id = 0
    self.browser_options = []


class MockAbstractGpuTestClass(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def GenerateGpuTests(cls, options):
    return []

  def RunActualGpuTest(self, test_path, *args):
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


def _checkTestExpectationsAreForExistingTests(
    test_class, mock_options, test_names=None):
  test_names = test_names or [
      args[0] for args in
      test_class.GenerateGpuTests(mock_options)]
  expectations_file = test_class.ExpectationsFiles()[0]
  trie = {}
  for test in test_names:
    _trie = trie.setdefault(test[0], {})
    for l in test[1:]:
      _trie = _trie.setdefault(l, {})
  f = open(expectations_file, 'r')
  expectations_file = os.path.basename(expectations_file)
  expectations = f.read()
  f.close()
  parser = expectations_parser.TaggedTestListParser(expectations)
  for exp in parser.expectations:
    _trie = trie
    for l in exp.test:
      if l == '*':
        break
      assert l in _trie, (
        "%s:%d: Glob '%s' does not match with any tests in the %s test suite" %
        (expectations_file, exp.lineno, exp.test, test_class.Name()))
      _trie = _trie[l]


def is_collision(s1, s2):
  # s1 collides with s2 when s1 is a subset of s2
  # A tag is in both sets if its a generic tag
  # and its in one set while a specifc tag covered by the generic tag is in
  # the other set.
  for tag in s1:
    if (not tag in s2 and not (
        tag in GENERIC_CONDITIONS and
        any(_map_specific_to_generic.get(t, t) == tag for t in s2)) and
        not _map_specific_to_generic.get(tag, tag) in s2):
      return False
  return True


def _glob_conflicts_with_exp(possible_collision, exp):
  reason_template = (
      'Pattern \'{0}\' on line {1} has the %s expectation however the '
      'the pattern on \'{2}\' line {3} has the Pass expectation'). format(
          possible_collision.test, possible_collision.lineno, exp.test,
          exp.lineno)
  causes_regression = not(
      ResultType.Failure in exp.results or ResultType.Skip in exp.results)
  if (ResultType.Skip in possible_collision.results and
      causes_regression):
    return reason_template % 'Skip'
  if (ResultType.Failure in possible_collision.results and
      causes_regression):
    return reason_template % 'Failure'
  return ''


def _map_gpu_devices_to_vendors(tag_sets):
  for tag_set in tag_sets:
    if any(gpu in tag_set for gpu in GPU_CONDITIONS):
      _map_specific_to_generic.update(
          {t[0]: t[1] for t in
           itertools.permutations(tag_set, 2) if (t[0] + '-').startswith(t[1])})
      break


def _checkTestExpectationGlobsForCollision(expectations, file_name):
  """This function looks for collisions between test expectations with patterns
  that match with test expectation patterns that are globs. A test expectation
  collides with another if its pattern matches with another's glob and if its
  tags is a super set of the other expectation's tags. The less specific test
  expectation must have a failure or skip expectation while the more specific
  test expectation does not. The more specific test expectation will trump
  the less specific test expectation and may cause an unexpected regression.

  Args:
  expectations: A string containing test expectations in the new format
  file_name: Name of the file that the test expectations came from
  """
  master_conflicts_found = False
  error_msg = ''
  trie = {}
  parser = expectations_parser.TaggedTestListParser(expectations)
  globs_to_expectations = defaultdict(list)

  _map_gpu_devices_to_vendors(parser.tag_sets)

  for exp in parser.expectations:
    _trie = trie.setdefault(exp.test[0], {})
    for l in exp.test[1:]:
      _trie = _trie.setdefault(l, {})
    _trie.setdefault('$', []).append(exp)

  for exp in parser.expectations:
    _trie = trie
    glob = ''
    for l in exp.test:
      if '*' in _trie:
        globs_to_expectations[glob + '*'].append(exp)
      glob += l
      if l in _trie:
        _trie = _trie[l]
      else:
        break
    if '*' in _trie:
      globs_to_expectations[glob + '*'].append(exp)

  for glob, expectations in globs_to_expectations.items():
    conflicts_found = False
    globs_to_match = [e for e in expectations if e.test == glob]
    matched_to_globs = [e for e in expectations if e.test != glob]
    for match in matched_to_globs:
      for possible_collision in globs_to_match:
        reason = _glob_conflicts_with_exp(possible_collision, match)
        if (reason and
            is_collision(possible_collision.tags, match.tags)):
          if not conflicts_found:
            error_msg += ('\n\nFound conflicts for pattern %s in %s:\n' %
                          (glob, file_name))
          master_conflicts_found = conflicts_found = True
          error_msg += ('  line %d conflicts with line %d: %s\n' %
                        (possible_collision.lineno, match.lineno, reason))
  assert not master_conflicts_found, error_msg


def _checkTestExpectationPatternsForCollision(expectations, file_name):
  """This function makes sure that any test expectations that have the same
  pattern do not collide with each other. They collide when one expectation's
  tags are a subset of the other expectation's tags. If the expectation with
  the larger tag set is active during a test run, then the expectation's whose
  tag set is a subset of the tags will be active as well.

  Args:
  expectations: A string containing test expectations in the new format
  file_name: Name of the file that the test expectations came from
  """
  parser = expectations_parser.TaggedTestListParser(expectations)
  tests_to_exps = defaultdict(list)
  master_conflicts_found = False
  error_msg = ''
  _map_gpu_devices_to_vendors(parser.tag_sets)

  for exp in parser.expectations:
    tests_to_exps[exp.test].append(exp)
  for pattern, exps in tests_to_exps.items():
    conflicts_found = False
    for e1, e2 in itertools.combinations(exps, 2):
      if is_collision(e1.tags, e2.tags) or is_collision(e2.tags, e1.tags):
        if not conflicts_found:
          error_msg += ('\n\nFound conflicts for test %s in %s:\n' %
                        (pattern, file_name))
        master_conflicts_found = conflicts_found = True
        error_msg += ('  line %d conflicts with line %d\n' %
                      (e1.lineno, e2.lineno))
  assert not master_conflicts_found, error_msg


def _CheckPatternIsValid(pattern):
  if not '*' in pattern and not 'WebglExtension_' in pattern:
    full_path = os.path.normpath(os.path.join(
        webgl_test_util.conformance_path, pattern))
    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified in ' +
        'expectation does not exist: ' + full_path)


def _testCheckTestExpectationsAreForExistingTests(expectations):
  options = MockArgs()
  expectations_file = tempfile.NamedTemporaryFile(delete=False)
  expectations_file.write(expectations)
  expectations_file.close()

  class _MockTestCase(object):
    @classmethod
    def Name(cls):
      return '_MockTestCase'

    @classmethod
    def GenerateGpuTests(cls, options):
      tests = [('a/b/c', ())]
      for t in tests:
        yield t

    @classmethod
    def ExpectationsFiles(cls):
      return [expectations_file.name]

  _checkTestExpectationsAreForExistingTests(_MockTestCase, options)


def _FindTestCases():
  test_cases = []
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=gpu_integration_test.GpuIntegrationTest)
    test_cases.extend(modules_to_classes.values())
  return test_cases


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

  def testCollisionInTestExpectationsWithSpecifcAndGenericOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenSpecificOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win7 ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    [ nvidia win ] a/b/c/d [ Failure ]
    [ nvidia-0x01 xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags2(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win ] a/b/c/* [ Failure ]
    [ nvidia win7 debug ] a/b/c/* [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenGpuDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win7 ] a/b/c/d [ Failure ]
    [ nvidia-0x02 win7 debug ] a/b/c/d [ Skip ]
    [ nvidia win debug ] a/b/c/* [ Skip ]
    '''
    _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testMixGenericandSpecificTagsInCollidingSets(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win ] a/b/c/d [ Failure ]
    [ nvidia win7 debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationCausesAssertion(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win ] a/b/c/d [ Failure ]
    [ intel win debug ] a/b/c/d [ Skip ]
    [ intel  ] a/b/c/d [ Failure ]
    [ amd mac ] a/b [ RetryOnFailure ]
    [ mac ] a/b [ Skip ]
    [ amd mac ] a/b/c [ Failure ]
    [ intel mac ] a/b/c [ Failure ]
    '''
    with self.assertRaises(AssertionError) as context:
      _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')
    self.assertIn("Found conflicts for test a/b/c/d in test.txt:",
      str(context.exception))
    self.assertIn('line 4 conflicts with line 5',
      str(context.exception))
    self.assertIn('line 4 conflicts with line 6',
      str(context.exception))
    self.assertIn('line 5 conflicts with line 6',
      str(context.exception))
    self.assertIn("Found conflicts for test a/b in test.txt:",
      str(context.exception))
    self.assertIn('line 7 conflicts with line 8',
      str(context.exception))
    self.assertNotIn("Found conflicts for test a/b/c in test.txt:",
      str(context.exception))

  def testCollisionWithGlobsWithFailureExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug ] a/b/c/d* [ Failure ]
    [ intel debug mac ] a/b/c/d [ RetryOnFailure ]
    [ intel debug mac ] a/b/c/d/e [ Failure ]
    '''
    with self.assertRaises(AssertionError) as context:
      _checkTestExpectationGlobsForCollision(test_expectations, 'test.txt')
    self.assertIn('Found conflicts for pattern a/b/c/d* in test.txt:',
        str(context.exception))
    self.assertIn(("line 4 conflicts with line 5: Pattern 'a/b/c/d*' on line 4 "
        "has the Failure expectation however the the pattern on 'a/b/c/d'"
        " line 5 has the Pass expectation"),
        str(context.exception))
    self.assertNotIn('line 4 conflicts with line 6',
        str(context.exception))

  def testNoCollisionWithGlobsWithFailureExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug mac ] a/b/c/* [ Failure ]
    [ intel debug ] a/b/c/d [ Failure ]
    [ intel debug ] a/b/c/d [ Skip ]
    '''
    _checkTestExpectationGlobsForCollision(test_expectations, 'test.txt')

  def testCollisionWithGlobsWithSkipExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug ] a/b/c/d* [ Skip ]
    [ intel debug mac ] a/b/c/d [ Failure ]
    [ intel debug mac ] a/b/c/d/e [ RetryOnFailure ]
    '''
    with self.assertRaises(AssertionError) as context:
      _checkTestExpectationGlobsForCollision(test_expectations, 'test.txt')
    self.assertIn('Found conflicts for pattern a/b/c/d* in test.txt:',
        str(context.exception))
    self.assertNotIn('line 4 conflicts with line 5',
        str(context.exception))
    self.assertIn('line 4 conflicts with line 6',
        str(context.exception))
    self.assertIn(("line 4 conflicts with line 6: Pattern 'a/b/c/d*' on line 4 "
        "has the Skip expectation however the the pattern on 'a/b/c/d/e'"
        " line 6 has the Pass expectation"),
        str(context.exception))

  def testNoCollisionWithGlobsWithSkipExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug mac ] a/b/c/* [ Skip ]
    [ intel debug ] a/b/c/d [ Skip ]
    [ intel debug ] a/b/c/d [ Skip ]
    '''
    _checkTestExpectationGlobsForCollision(test_expectations, 'test.txt')

  def testNoCollisionInTestExpectations(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win release ] a/b/* [ Failure ]
    [ intel debug ] a/b/c/d [ Failure ]
    [ nvidia debug ] a/b/c/d [ Failure ]
    '''
    _checkTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionsForSameTestsInGpuTestExpectations(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for i in xrange(1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(test_case.GenerateGpuTests(
              MockArgs(webgl_version=('%d.0.0' % i))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              _checkTestExpectationPatternsForCollision(f.read(),
              os.path.basename(f.name))

  def testNoCollisionsWithGlobsInGpuTestExpectations(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for i in xrange(1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(test_case.GenerateGpuTests(
              MockArgs(webgl_version=('%d.0.0' % i))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              _checkTestExpectationGlobsForCollision(f.read(),
              os.path.basename(f.name))

  def testGpuTestExpectationsAreForExistingTests(self):
    options = MockArgs()
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        if (test_case.Name() not in ('pixel', 'webgl_conformance')
            and test_case.ExpectationsFiles()):
          _checkTestExpectationsAreForExistingTests(test_case, options)

  def testWebglTestPathsExist(self):
    webgl_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if test_case == webgl_test_class:
        for i in xrange(1, 3):
          _ = list(test_case.GenerateGpuTests(
              MockArgs(webgl_version='%d.0.0' % i)))
          with open(test_case.ExpectationsFiles()[0], 'r') as f:
            expectations = expectations_parser.TaggedTestListParser(f.read())
            for exp in expectations.expectations:
              _CheckPatternIsValid(exp.test)

  def testPixelTestsExpectationsAreForExistingTests(self):
    pixel_test_names = []
    for _, method in inspect.getmembers(
        pixel_test_pages.PixelTestPages, predicate=inspect.isfunction):
      pixel_test_names.extend(
          [p.name for p in method(
              pixel_integration_test.PixelIntegrationTest.test_base_name)])
    _checkTestExpectationsAreForExistingTests(
        pixel_integration_test.PixelIntegrationTest,
        MockArgs(), pixel_test_names)

  def testExpectationPatternNotInGeneratedTests(self):
    with self.assertRaises(AssertionError) as context:
      _testCheckTestExpectationsAreForExistingTests('a/b/d [ Failure ]')
    self.assertIn(("1: Glob 'a/b/d' does not match with any"
                  " tests in the _MockTestCase test suite"),
                  str(context.exception))

  def testGlobMatchesTestName(self):
    _testCheckTestExpectationsAreForExistingTests('a/b* [ Failure ]')

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
      ['unexpected_error',
       'unexpected_failure'],
      ['expected_flaky',
       'expected_failure'],
      ['expected_skip'],
      ['--retry-only-retry-on-failure', '--retry-limit=3',
      '--test-name-prefix=unittest_data.integration_tests.SimpleTest.'])
    # The number of browser starts include the one call to StartBrowser at the
    # beginning of the run of the test suite and for each RestartBrowser call
    # which happens after every failure
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
      ['a/b/unexpected-fail.html'],
      ['a/b/expected-fail.html', 'a/b/expected-flaky.html'],
      ['should_skip'],
      ['--retry-limit=3', '--retry-only-retry-on-failure-tests',
       ('--test-name-prefix=unittest_data.integration_tests.'
        'RunTestsWithExpectationsFiles.')])

  def testTestFilterCommandLineArg(self):
    self._RunIntegrationTest(
      'run_tests_with_expectations_files',
      ['a/b/unexpected-fail.html'],
      ['a/b/expected-fail.html'],
      ['should_skip'],
      ['--retry-limit=3', '--retry-only-retry-on-failure-tests',
       ('--test-filter=a/b/unexpected-fail.html::a/b/expected-fail.html::'
        'should_skip'),
       ('--test-name-prefix=unittest_data.integration_tests.'
        'RunTestsWithExpectationsFiles.')])

  def testUseTestExpectationsFileToHandleExpectedSkip(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['should_skip']
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
      ['skip', 'flaky'],
      # Tests that are expected to fail and do fail are treated as test passes
      ['expected_failure'],
      [],
      ['--all', '--test-name-prefix',
      'unittest_data.integration_tests.TestAlsoRunDisabledTests.',
      '--retry-limit=3', '--retry-only-retry-on-failure'])
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

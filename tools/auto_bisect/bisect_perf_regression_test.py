# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import unittest

import bisect_perf_regression
import bisect_results
import source_control as source_control_module

def _GetBisectPerformanceMetricsInstance():
  """Returns an instance of the BisectPerformanceMetrics class."""
  options_dict = {
    'debug_ignore_build': True,
    'debug_ignore_sync': True,
    'debug_ignore_perf_test': True,
    'command': 'fake_command',
    'metric': 'fake/metric',
    'good_revision': 280000,
    'bad_revision': 280005,
  }
  bisect_options = bisect_perf_regression.BisectOptions.FromDict(options_dict)
  source_control = source_control_module.DetermineAndCreateSourceControl(
      bisect_options)
  bisect_instance = bisect_perf_regression.BisectPerformanceMetrics(
      source_control, bisect_options)
  return bisect_instance


class BisectPerfRegressionTest(unittest.TestCase):
  """Test case for other functions and classes in bisect-perf-regression.py."""

  def setUp(self):
    self.cwd = os.getcwd()
    os.chdir(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                          os.path.pardir, os.path.pardir)))

  def tearDown(self):
    os.chdir(self.cwd)

  def _AssertConfidence(self, score, bad_values, good_values):
    """Checks whether the given sets of values have a given confidence score.

    The score represents our confidence that the two sets of values wouldn't
    be as different as they are just by chance; that is, that some real change
    occurred between the two sets of values.

    Args:
      score: Expected confidence score.
      bad_values: First list of numbers.
      good_values: Second list of numbers.
    """
    # ConfidenceScore takes a list of lists but these lists are flattened
    # inside the function.
    confidence = bisect_results.ConfidenceScore(
        [[v] for v in bad_values],
        [[v] for v in good_values])
    self.assertEqual(score, confidence)

  def testConfidenceScore_ZeroConfidence(self):
    # The good and bad sets contain the same values, so the confidence that
    # they're different should be zero.
    self._AssertConfidence(0.0, [4, 5, 7, 6, 8, 7], [8, 7, 6, 7, 5, 4])

  def testConfidenceScore_MediumConfidence(self):
    self._AssertConfidence(80.0, [0, 1, 1, 1, 2, 2], [1, 1, 1, 3, 3, 4])

  def testConfidenceScore_HighConfidence(self):
    self._AssertConfidence(95.0, [0, 1, 1, 1, 2, 2], [1, 2, 2, 3, 3, 4])

  def testConfidenceScore_VeryHighConfidence(self):
    # Confidence is high if the two sets of values have no internal variance.
    self._AssertConfidence(99.9, [1, 1, 1, 1], [1.2, 1.2, 1.2, 1.2])
    self._AssertConfidence(99.9, [1, 1, 1, 1], [1.01, 1.01, 1.01, 1.01])

  def testConfidenceScore_UnbalancedSampleSize(self):
    # The second set of numbers only contains one number, so confidence is 0.
    self._AssertConfidence(0.0, [1.1, 1.2, 1.1, 1.2, 1.0, 1.3, 1.2], [1.4])

  def testConfidenceScore_EmptySample(self):
    # Confidence is zero if either or both samples are empty.
    self._AssertConfidence(0.0, [], [])
    self._AssertConfidence(0.0, [], [1.1, 1.2, 1.1, 1.2, 1.0, 1.3, 1.2, 1.3])
    self._AssertConfidence(0.0, [1.1, 1.2, 1.1, 1.2, 1.0, 1.3, 1.2, 1.3], [])

  def testConfidenceScore_FunctionalTestResults(self):
    self._AssertConfidence(80.0, [1, 1, 0, 1, 1, 1, 0, 1], [0, 0, 1, 0, 1, 0])
    self._AssertConfidence(99.9, [1, 1, 1, 1, 1, 1, 1, 1], [0, 0, 0, 0, 0, 0])

  def testConfidenceScore_RealWorldCases(self):
    """This method contains a set of data from actual bisect results.

    The confidence scores asserted below were all copied from the actual
    results, so the purpose of this test method is mainly to show what the
    results for real cases are, and compare when we change the confidence
    score function in the future.
    """
    self._AssertConfidence(80, [133, 130, 132, 132, 130, 129], [129, 129, 125])
    self._AssertConfidence(99.5, [668, 667], [498, 498, 499])
    self._AssertConfidence(80, [67, 68], [65, 65, 67])
    self._AssertConfidence(0, [514], [514])
    self._AssertConfidence(90, [616, 613, 607, 615], [617, 619, 619, 617])
    self._AssertConfidence(0, [3.5, 5.8, 4.7, 3.5, 3.6], [2.8])
    self._AssertConfidence(90, [3, 3, 3], [2, 2, 2, 3])
    self._AssertConfidence(0, [1999004, 1999627], [223355])
    self._AssertConfidence(90, [1040, 934, 961], [876, 875, 789])
    self._AssertConfidence(90, [309, 305, 304], [302, 302, 299, 303, 298])

  def testParseDEPSStringManually(self):
    """Tests DEPS parsing."""
    deps_file_contents = """
    vars = {
        'ffmpeg_hash':
             '@ac4a9f31fe2610bd146857bbd55d7a260003a888',
        'webkit_url':
             'https://chromium.googlesource.com/chromium/blink.git',
        'git_url':
             'https://chromium.googlesource.com',
        'webkit_rev':
             '@e01ac0a267d1017288bc67fa3c366b10469d8a24',
        'angle_revision':
             '74697cf2064c0a2c0d7e1b1b28db439286766a05'
    }"""

    # Should only expect SVN/git revisions to come through, and URLs should be
    # filtered out.
    expected_vars_dict = {
        'ffmpeg_hash': '@ac4a9f31fe2610bd146857bbd55d7a260003a888',
        'webkit_rev': '@e01ac0a267d1017288bc67fa3c366b10469d8a24',
        'angle_revision': '74697cf2064c0a2c0d7e1b1b28db439286766a05'
    }
    # Testing private function.
    # pylint: disable=W0212
    vars_dict = bisect_perf_regression._ParseRevisionsFromDEPSFileManually(
        deps_file_contents)
    self.assertEqual(vars_dict, expected_vars_dict)

  def _AssertParseResult(self, expected_values, result_string):
    """Asserts some values are parsed from a RESULT line."""
    results_template = ('RESULT other_chart: other_trace= 123 count\n'
                        'RESULT my_chart: my_trace= %(value)s\n')
    results = results_template % {'value': result_string}
    metric = ['my_chart', 'my_trace']
    # Testing private function.
    # pylint: disable=W0212
    values = bisect_perf_regression._TryParseResultValuesFromOutput(
        metric, results)
    self.assertEqual(expected_values, values)

  def testTryParseResultValuesFromOutput_WithSingleValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= <value>"""
    self._AssertParseResult([66.88], '66.88 kb')
    self._AssertParseResult([66.88], '66.88 ')
    self._AssertParseResult([-66.88], '-66.88 kb')
    self._AssertParseResult([66], '66 kb')
    self._AssertParseResult([0.66], '.66 kb')
    self._AssertParseResult([], '. kb')
    self._AssertParseResult([], 'aaa kb')

  def testTryParseResultValuesFromOutput_WithMultiValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= [<value>,<value>, ..]"""
    self._AssertParseResult([66.88], '[66.88] kb')
    self._AssertParseResult([66.88, 99.44], '[66.88, 99.44]kb')
    self._AssertParseResult([66.88, 99.44], '[ 66.88, 99.44 ]')
    self._AssertParseResult([-66.88, 99.44], '[-66.88, 99.44] kb')
    self._AssertParseResult([-66, 99], '[-66,99] kb')
    self._AssertParseResult([-66, 99], '[-66,99,] kb')
    self._AssertParseResult([-66, 0.99], '[-66,.99] kb')
    self._AssertParseResult([], '[] kb')
    self._AssertParseResult([], '[-66,abc] kb')

  def testTryParseResultValuesFromOutputWithMeanStd(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= {<mean, std}"""
    self._AssertParseResult([33.22], '{33.22, 3.6} kb')
    self._AssertParseResult([33.22], '{33.22, 3.6} kb')
    self._AssertParseResult([33.22], '{33.22,3.6}kb')
    self._AssertParseResult([33.22], '{33.22,3.6} kb')
    self._AssertParseResult([33.22], '{ 33.22,3.6 }kb')
    self._AssertParseResult([-33.22], '{-33.22,3.6}kb')
    self._AssertParseResult([22], '{22,6}kb')
    self._AssertParseResult([.22], '{.22,6}kb')
    self._AssertParseResult([], '{.22,6, 44}kb')
    self._AssertParseResult([], '{}kb')
    self._AssertParseResult([], '{XYZ}kb')

  def _AssertCompatibleCommand(
      self, expected_command, original_command, revision, target_platform):
    """Tests the modification of the command that might be done.

    This modification to the command is done in order to get a Telemetry
    command that works; before some revisions, the browser name that Telemetry
    expects is different in some cases, but we want it to work anyway.

    Specifically, only for android:
      After r276628, only android-chrome-shell works.
      Prior to r274857, only android-chromium-testshell works.
      In the range [274857, 276628], both work.
    """
    bisect_options = bisect_perf_regression.BisectOptions()
    bisect_options.output_buildbot_annotations = None
    source_control = source_control_module.DetermineAndCreateSourceControl(
        bisect_options)
    bisect_instance = bisect_perf_regression.BisectPerformanceMetrics(
        source_control, bisect_options)
    bisect_instance.opts.target_platform = target_platform
    git_revision = bisect_instance.source_control.ResolveToRevision(
        revision, 'chromium', bisect_perf_regression.DEPOT_DEPS_NAME, 100)
    depot = 'chromium'
    command = bisect_instance.GetCompatibleCommand(
        original_command, git_revision, depot)
    self.assertEqual(expected_command, command)

  def testGetCompatibleCommand_ChangeToTestShell(self):
    # For revisions <= r274857, only android-chromium-testshell is used.
    self._AssertCompatibleCommand(
        'tools/perf/run_benchmark -v --browser=android-chromium-testshell foo',
        'tools/perf/run_benchmark -v --browser=android-chrome-shell foo',
        274857, 'android')

  def testGetCompatibleCommand_ChangeToShell(self):
    # For revisions >= r276728, only android-chrome-shell can be used.
    self._AssertCompatibleCommand(
        'tools/perf/run_benchmark -v --browser=android-chrome-shell foo',
        'tools/perf/run_benchmark -v --browser=android-chromium-testshell foo',
        276628, 'android')

  def testGetCompatibleCommand_NoChange(self):
    # For revisions < r276728, android-chromium-testshell can be used.
    self._AssertCompatibleCommand(
        'tools/perf/run_benchmark -v --browser=android-chromium-testshell foo',
        'tools/perf/run_benchmark -v --browser=android-chromium-testshell foo',
        274858, 'android')
    # For revisions > r274857, android-chrome-shell can be used.
    self._AssertCompatibleCommand(
        'tools/perf/run_benchmark -v --browser=android-chrome-shell foo',
        'tools/perf/run_benchmark -v --browser=android-chrome-shell foo',
        274858, 'android')

  def testGetCompatibleCommand_NonAndroidPlatform(self):
    # In most cases, there's no need to change Telemetry command.
    # For revisions >= r276728, only android-chrome-shell can be used.
    self._AssertCompatibleCommand(
        'tools/perf/run_benchmark -v --browser=release foo',
        'tools/perf/run_benchmark -v --browser=release foo',
        276628, 'chromium')

  # This method doesn't reference self; it fails if an error is thrown.
  # pylint: disable=R0201
  def testDryRun(self):
    """Does a dry run of the bisect script.

    This serves as a smoke test to catch errors in the basic execution of the
    script.
    """
    # Disable rmtree to avoid deleting local trees.
    old_rmtree = shutil.rmtree
    try:
      shutil.rmtree = lambda path, onerror: None
      bisect_instance = _GetBisectPerformanceMetricsInstance()
      results = bisect_instance.Run(bisect_instance.opts.command,
                                    bisect_instance.opts.bad_revision,
                                    bisect_instance.opts.good_revision,
                                    bisect_instance.opts.metric)
      bisect_instance.FormatAndPrintResults(results)
    finally:
      shutil.rmtree = old_rmtree

  def testGetCommitPosition(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance()
    cp_git_rev = '7017a81991de983e12ab50dfc071c70e06979531'
    self.assertEqual(
        291765, bisect_instance.source_control.GetCommitPosition(cp_git_rev))

    svn_git_rev = 'e6db23a037cad47299a94b155b95eebd1ee61a58'
    self.assertEqual(
        291467, bisect_instance.source_control.GetCommitPosition(svn_git_rev))

  def testGetCommitPositionForV8(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance()
    v8_rev = '21d700eedcdd6570eff22ece724b63a5eefe78cb'
    depot_path = os.path.join(bisect_instance.src_cwd, 'v8')
    self.assertEqual(
        23634,
        bisect_instance.source_control.GetCommitPosition(v8_rev, depot_path))

  def testGetCommitPositionForWebKit(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance()
    wk_rev = 'a94d028e0f2c77f159b3dac95eb90c3b4cf48c61'
    depot_path = os.path.join(bisect_instance.src_cwd, 'third_party', 'WebKit')
    self.assertEqual(
        181660,
        bisect_instance.source_control.GetCommitPosition(wk_rev, depot_path))

  def testUpdateDepsContent(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance()
    deps_file = 'DEPS'
    # We are intentionally reading DEPS file contents instead of string literal
    # with few lines from DEPS because to check if the format we are expecting
    # to search is not changed in DEPS content.
    # TODO (prasadv): Add a separate test to validate the DEPS contents with the
    # format that bisect script expects.
    deps_contents = bisect_perf_regression.ReadStringFromFile(deps_file)
    deps_key = 'v8_revision'
    depot = 'v8'
    git_revision = 'a12345789a23456789a123456789a123456789'
    updated_content = bisect_instance.UpdateDepsContents(
        deps_contents, depot, git_revision, deps_key)
    self.assertIsNotNone(updated_content)
    ss = re.compile('["\']%s["\']: ["\']%s["\']' % (deps_key, git_revision))
    self.assertIsNotNone(re.search(ss, updated_content))


class DepotDirectoryRegistryTest(unittest.TestCase):

  def setUp(self):
    self.old_chdir = os.chdir
    os.chdir = self.mockChdir
    self.old_depot_names = bisect_perf_regression.DEPOT_NAMES
    bisect_perf_regression.DEPOT_NAMES = ['mock_depot']
    self.old_depot_deps_name = bisect_perf_regression.DEPOT_DEPS_NAME
    bisect_perf_regression.DEPOT_DEPS_NAME = {'mock_depot': {'src': 'src/foo'}}

    self.registry = bisect_perf_regression.DepotDirectoryRegistry('/mock/src')
    self.cur_dir = None

  def tearDown(self):
    os.chdir = self.old_chdir
    bisect_perf_regression.DEPOT_NAMES = self.old_depot_names
    bisect_perf_regression.DEPOT_DEPS_NAME = self.old_depot_deps_name

  def mockChdir(self, new_dir):
    self.cur_dir = new_dir

  def testReturnsCorrectResultForChrome(self):
    self.assertEqual(self.registry.GetDepotDir('chromium'), '/mock/src')

  def testReturnsCorrectResultForChromeOS(self):
    self.assertEqual(self.registry.GetDepotDir('cros'), '/mock/src/tools/cros')

  def testUsesDepotSpecToInitializeRegistry(self):
    self.assertEqual(self.registry.GetDepotDir('mock_depot'), '/mock/src/foo')

  def testChangedTheDirectory(self):
    self.registry.ChangeToDepotDir('mock_depot')
    self.assertEqual(self.cur_dir, '/mock/src/foo')


if __name__ == '__main__':
  unittest.main()

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import sys
import unittest

SRC = os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir)
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))

import bisect_perf_regression
import bisect_utils
import mock
import source_control


# Regression confidence: 0%
CLEAR_NON_REGRESSION = [
    # Mean: 30.223 Std. Dev.: 11.383
    [[16.886], [16.909], [16.99], [17.723], [17.952], [18.118], [19.028],
     [19.552], [21.954], [38.573], [38.839], [38.965], [40.007], [40.572],
     [41.491], [42.002], [42.33], [43.109], [43.238]],
    # Mean: 34.76 Std. Dev.: 11.516
    [[16.426], [17.347], [20.593], [21.177], [22.791], [27.843], [28.383],
     [28.46], [29.143], [40.058], [40.303], [40.558], [41.918], [42.44],
     [45.223], [46.494], [50.002], [50.625], [50.839]]
]
# Regression confidence: ~ 90%
ALMOST_REGRESSION = [
    # Mean: 30.042 Std. Dev.: 2.002
    [[26.146], [28.04], [28.053], [28.074], [28.168], [28.209], [28.471],
     [28.652], [28.664], [30.862], [30.973], [31.002], [31.897], [31.929],
     [31.99], [32.214], [32.323], [32.452], [32.696]],
    # Mean: 33.008 Std. Dev.: 4.265
    [[34.963], [30.741], [39.677], [39.512], [34.314], [31.39], [34.361],
     [25.2], [30.489], [29.434]]
]
# Regression confidence: ~ 98%
BARELY_REGRESSION = [
    # Mean: 28.828 Std. Dev.: 1.993
    [[26.96], [27.605], [27.768], [27.829], [28.006], [28.206], [28.393],
     [28.911], [28.933], [30.38], [30.462], [30.808], [31.74], [31.805],
     [31.899], [32.077], [32.454], [32.597], [33.155]],
    # Mean: 31.156 Std. Dev.: 1.980
    [[28.729], [29.112], [29.258], [29.454], [29.789], [30.036], [30.098],
     [30.174], [30.534], [32.285], [32.295], [32.552], [32.572], [32.967],
     [33.165], [33.403], [33.588], [33.744], [34.147], [35.84]]
]
# Regression confidence: 99.5%
CLEAR_REGRESSION = [
    # Mean: 30.254 Std. Dev.: 2.987
    [[26.494], [26.621], [26.701], [26.997], [26.997], [27.05], [27.37],
     [27.488], [27.556], [31.846], [32.192], [32.21], [32.586], [32.596],
     [32.618], [32.95], [32.979], [33.421], [33.457], [34.97]],
    # Mean: 33.190 Std. Dev.: 2.972
    [[29.547], [29.713], [29.835], [30.132], [30.132], [30.33], [30.406],
     [30.592], [30.72], [34.486], [35.247], [35.253], [35.335], [35.378],
     [35.934], [36.233], [36.41], [36.947], [37.982]]
]
# Default options for the dry run
DEFAULT_OPTIONS = {
    'debug_ignore_build': True,
    'debug_ignore_sync': True,
    'debug_ignore_perf_test': True,
    'debug_ignore_regression_confidence': True,
    'command': 'fake_command',
    'metric': 'fake/metric',
    'good_revision': 280000,
    'bad_revision': 280005,
}

# This global is a placeholder for a generator to be defined by the testcases
# that use _MockRunTest
_MockResultsGenerator = (x for x in [])

def _FakeTestResult(values):
  result_dict = {'mean': 0.0, 'std_err': 0.0, 'std_dev': 0.0, 'values': values}
  success_code = 0
  return (result_dict, success_code)


def _MockRunTests(*args, **kwargs):
  _, _ = args, kwargs
  return _FakeTestResult(_MockResultsGenerator.next())


def _GetBisectPerformanceMetricsInstance(options_dict):
  """Returns an instance of the BisectPerformanceMetrics class."""
  opts = bisect_perf_regression.BisectOptions.FromDict(options_dict)
  return bisect_perf_regression.BisectPerformanceMetrics(opts, os.getcwd())


def _GetExtendedOptions(improvement_dir, fake_first, ignore_confidence=True):
  """Returns the a copy of the default options dict plus some options."""
  result = dict(DEFAULT_OPTIONS)
  result.update({
      'improvement_direction': improvement_dir,
      'debug_fake_first_test_mean': fake_first,
      'debug_ignore_regression_confidence': ignore_confidence})
  return result


def _GenericDryRun(options, print_results=False):
  """Performs a dry run of the bisector.

  Args:
    options: Dictionary containing the options for the bisect instance.
    print_results: Boolean telling whether to call FormatAndPrintResults.

  Returns:
    The results dictionary as returned by the bisect Run method.
  """
  # Disable rmtree to avoid deleting local trees.
  old_rmtree = shutil.rmtree
  try:
    shutil.rmtree = lambda path, onerror: None
    bisect_instance = _GetBisectPerformanceMetricsInstance(options)
    results = bisect_instance.Run(
        bisect_instance.opts.command, bisect_instance.opts.bad_revision,
        bisect_instance.opts.good_revision, bisect_instance.opts.metric)

    if print_results:
      bisect_instance.printer.FormatAndPrintResults(results)

    return results
  finally:
    shutil.rmtree = old_rmtree


class BisectPerfRegressionTest(unittest.TestCase):
  """Test case for other functions and classes in bisect-perf-regression.py."""

  def setUp(self):
    self.cwd = os.getcwd()
    os.chdir(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                          os.path.pardir, os.path.pardir)))

  def tearDown(self):
    os.chdir(self.cwd)

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
    bisect_instance = bisect_perf_regression.BisectPerformanceMetrics(
        bisect_options, os.getcwd())
    bisect_instance.opts.target_platform = target_platform
    git_revision = source_control.ResolveToRevision(
        revision, 'chromium', bisect_utils.DEPOT_DEPS_NAME, 100)
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
    _GenericDryRun(DEFAULT_OPTIONS, True)

  def testBisectImprovementDirectionFails(self):
    """Dry run of a bisect with an improvement instead of regression."""
    # Test result goes from 0 to 100 where higher is better
    results = _GenericDryRun(_GetExtendedOptions(1, 100))
    self.assertIsNotNone(results.error)
    self.assertIn('not a regression', results.error)

    # Test result goes from 0 to -100 where lower is better
    results = _GenericDryRun(_GetExtendedOptions(-1, -100))
    self.assertIsNotNone(results.error)
    self.assertIn('not a regression', results.error)

  def testBisectImprovementDirectionSucceeds(self):
    """Bisects with improvement direction matching regression range."""
    # Test result goes from 0 to 100 where lower is better
    results = _GenericDryRun(_GetExtendedOptions(-1, 100))
    self.assertIsNone(results.error)
    # Test result goes from 0 to -100 where higher is better
    results = _GenericDryRun(_GetExtendedOptions(1, -100))
    self.assertIsNone(results.error)

  @mock.patch('bisect_perf_regression.BisectPerformanceMetrics.'
              'RunPerformanceTestAndParseResults', _MockRunTests)
  def testBisectStopsOnDoubtfulRegression(self):
    global _MockResultsGenerator
    _MockResultsGenerator = (rs for rs in CLEAR_NON_REGRESSION)
    results = _GenericDryRun(_GetExtendedOptions(0, 0, False))
    confidence_warnings = [x for x in results.warnings if x.startswith(
        '\nWe could not reproduce the regression')]
    self.assertGreater(len(confidence_warnings), 0)

    _MockResultsGenerator = (rs for rs in ALMOST_REGRESSION)
    results = _GenericDryRun(_GetExtendedOptions(0, 0, False))
    confidence_warnings = [x for x in results.warnings if x.startswith(
        '\nWe could not reproduce the regression')]
    self.assertGreater(len(confidence_warnings), 0)

  @mock.patch('bisect_perf_regression.BisectPerformanceMetrics.'
              'RunPerformanceTestAndParseResults', _MockRunTests)
  def testBisectContinuesOnClearRegression(self):
    global _MockResultsGenerator
    _MockResultsGenerator = (rs for rs in CLEAR_REGRESSION)
    with self.assertRaises(StopIteration):
      _GenericDryRun(_GetExtendedOptions(0, 0, False))

    _MockResultsGenerator = (rs for rs in BARELY_REGRESSION)
    with self.assertRaises(StopIteration):
      _GenericDryRun(_GetExtendedOptions(0, 0, False))

  def testGetCommitPosition(self):
    cp_git_rev = '7017a81991de983e12ab50dfc071c70e06979531'
    self.assertEqual(291765, source_control.GetCommitPosition(cp_git_rev))

    svn_git_rev = 'e6db23a037cad47299a94b155b95eebd1ee61a58'
    self.assertEqual(291467, source_control.GetCommitPosition(svn_git_rev))

  def testGetCommitPositionForV8(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance(DEFAULT_OPTIONS)
    v8_rev = '21d700eedcdd6570eff22ece724b63a5eefe78cb'
    depot_path = os.path.join(bisect_instance.src_cwd, 'v8')
    self.assertEqual(
        23634, source_control.GetCommitPosition(v8_rev, depot_path))

  def testGetCommitPositionForWebKit(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance(DEFAULT_OPTIONS)
    wk_rev = 'a94d028e0f2c77f159b3dac95eb90c3b4cf48c61'
    depot_path = os.path.join(bisect_instance.src_cwd, 'third_party', 'WebKit')
    self.assertEqual(
        181660, source_control.GetCommitPosition(wk_rev, depot_path))

  def testUpdateDepsContent(self):
    bisect_instance = _GetBisectPerformanceMetricsInstance(DEFAULT_OPTIONS)
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
    self.old_depot_names = bisect_utils.DEPOT_NAMES
    bisect_utils.DEPOT_NAMES = ['mock_depot']
    self.old_depot_deps_name = bisect_utils.DEPOT_DEPS_NAME
    bisect_utils.DEPOT_DEPS_NAME = {'mock_depot': {'src': 'src/foo'}}

    self.registry = bisect_perf_regression.DepotDirectoryRegistry('/mock/src')
    self.cur_dir = None

  def tearDown(self):
    os.chdir = self.old_chdir
    bisect_utils.DEPOT_NAMES = self.old_depot_names
    bisect_utils.DEPOT_DEPS_NAME = self.old_depot_deps_name

  def mockChdir(self, new_dir):
    self.cur_dir = new_dir

  def testReturnsCorrectResultForChrome(self):
    self.assertEqual(self.registry.GetDepotDir('chromium'), '/mock/src')

  def testUsesDepotSpecToInitializeRegistry(self):
    self.assertEqual(self.registry.GetDepotDir('mock_depot'), '/mock/src/foo')

  def testChangedTheDirectory(self):
    self.registry.ChangeToDepotDir('mock_depot')
    self.assertEqual(self.cur_dir, '/mock/src/foo')


# The tests below test private functions (W0212).
# pylint: disable=W0212
class GitTryJobTestCases(unittest.TestCase):

  """Test case for bisect try job."""
  def setUp(self):
    bisect_utils_patcher = mock.patch('bisect_perf_regression.bisect_utils')
    self.mock_bisect_utils = bisect_utils_patcher.start()
    self.addCleanup(bisect_utils_patcher.stop)

  def _SetupRunGitMock(self, git_cmds):
    """Setup RunGit mock with expected output for given git command."""
    def side_effect(git_cmd_args):
      for val in git_cmds:
        if set(val[0]) == set(git_cmd_args):
          return val[1]
    self.mock_bisect_utils.RunGit = mock.Mock(side_effect=side_effect)

  def _AssertRunGitExceptions(self, git_cmds, func, *args):
    """Setup RunGit mock and tests RunGitException.

    Args:
      git_cmds: List of tuples with git command and expected output.
      func: Callback function to be executed.
      args: List of arguments to be passed to the function.
    """
    self._SetupRunGitMock(git_cmds)
    self.assertRaises(bisect_perf_regression.RunGitError,
                      func,
                      *args)

  def testNotGitRepo(self):
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    cmds = [(['rev-parse', '--abbrev-ref', 'HEAD'], (None, 128))]
    self._AssertRunGitExceptions(cmds,
                                 bisect_perf_regression._PrepareBisectBranch,
                                 parent_branch, new_branch)

  def testFailedCheckoutMaster(self):
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    cmds = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (new_branch, 0)),
        (['checkout', '-f', parent_branch], ('Checkout Failed', 1)),
    ]
    self._AssertRunGitExceptions(cmds,
                                 bisect_perf_regression._PrepareBisectBranch,
                                 parent_branch, new_branch)

  def testDeleteBisectBranchIfExists(self):
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    cmds = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (parent_branch, 0)),
        (['branch', '--list'], ('bisect-tryjob\n*master\nsomebranch', 0)),
        (['branch', '-D', new_branch], ('Failed to delete branch', 128)),
    ]
    self._AssertRunGitExceptions(cmds,
                                 bisect_perf_regression._PrepareBisectBranch,
                                 parent_branch, new_branch)

  def testCreatNewBranchFails(self):
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    cmds = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (parent_branch, 0)),
        (['branch', '--list'], ('bisect-tryjob\n*master\nsomebranch', 0)),
        (['branch', '-D', new_branch], ('None', 0)),
        (['update-index', '--refresh', '-q'], (None, 0)),
        (['diff-index', 'HEAD'], (None, 0)),
        (['checkout', '-b', new_branch], ('Failed to create branch', 128)),
    ]
    self._AssertRunGitExceptions(cmds,
                                 bisect_perf_regression._PrepareBisectBranch,
                                 parent_branch, new_branch)

  def testSetUpstreamToFails(self):
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    cmds = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (parent_branch, 0)),
        (['branch', '--list'], ('bisect-tryjob\n*master\nsomebranch', 0)),
        (['branch', '-D', new_branch], ('None', 0)),
        (['update-index', '--refresh', '-q'], (None, 0)),
        (['diff-index', 'HEAD'], (None, 0)),
        (['checkout', '-b', new_branch], ('None', 0)),
        (['branch', '--set-upstream-to', parent_branch],
         ('Setuptream fails', 1)),
    ]
    self._AssertRunGitExceptions(cmds,
                                 bisect_perf_regression._PrepareBisectBranch,
                                 parent_branch, new_branch)

  def testBuilderTryJobForException(self):
    git_revision = 'ac4a9f31fe2610bd146857bbd55d7a260003a888'
    bot_name = 'linux_perf_bisect_builder'
    bisect_job_name = 'testBisectJobname'
    patch = None
    patch_content = '/dev/null'
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    try_cmd = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (parent_branch, 0)),
        (['branch', '--list'], ('bisect-tryjob\n*master\nsomebranch', 0)),
        (['branch', '-D', new_branch], ('None', 0)),
        (['update-index', '--refresh', '-q'], (None, 0)),
        (['diff-index', 'HEAD'], (None, 0)),
        (['checkout', '-b', new_branch], ('None', 0)),
        (['branch', '--set-upstream-to', parent_branch],
         ('Setuptream fails', 0)),
        (['try',
          '-b', bot_name,
          '-r', git_revision,
          '-n', bisect_job_name,
          '--svn_repo=%s' % bisect_perf_regression.SVN_REPO_URL,
          '--diff=%s' % patch_content
         ], (None, 1)),
    ]
    self._AssertRunGitExceptions(try_cmd,
                                 bisect_perf_regression._BuilderTryjob,
                                 git_revision, bot_name, bisect_job_name, patch)

  def testBuilderTryJob(self):
    git_revision = 'ac4a9f31fe2610bd146857bbd55d7a260003a888'
    bot_name = 'linux_perf_bisect_builder'
    bisect_job_name = 'testBisectJobname'
    patch = None
    patch_content = '/dev/null'
    new_branch = bisect_perf_regression.BISECT_TRYJOB_BRANCH
    parent_branch = bisect_perf_regression.BISECT_MASTER_BRANCH
    try_cmd = [
        (['rev-parse', '--abbrev-ref', 'HEAD'], (parent_branch, 0)),
        (['branch', '--list'], ('bisect-tryjob\n*master\nsomebranch', 0)),
        (['branch', '-D', new_branch], ('None', 0)),
        (['update-index', '--refresh', '-q'], (None, 0)),
        (['diff-index', 'HEAD'], (None, 0)),
        (['checkout', '-b', new_branch], ('None', 0)),
        (['branch', '--set-upstream-to', parent_branch],
         ('Setuptream fails', 0)),
        (['try',
          '-b', bot_name,
          '-r', git_revision,
          '-n', bisect_job_name,
          '--svn_repo=%s' % bisect_perf_regression.SVN_REPO_URL,
          '--diff=%s' % patch_content
         ], (None, 0)),
    ]
    self._SetupRunGitMock(try_cmd)
    bisect_perf_regression._BuilderTryjob(
        git_revision, bot_name, bisect_job_name, patch)


if __name__ == '__main__':
  unittest.main()


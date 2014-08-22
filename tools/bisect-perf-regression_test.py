# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from auto_bisect import source_control as source_control_module

# Special import necessary because filename contains dash characters.
bisect_perf_module = __import__('bisect-perf-regression')


class BisectPerfRegressionTest(unittest.TestCase):
  """Test case for other functions and classes in bisect-perf-regression.py."""

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
    confidence = bisect_perf_module.ConfidenceScore(
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
    vars_dict = bisect_perf_module._ParseRevisionsFromDEPSFileManually(
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
    values = bisect_perf_module._TryParseResultValuesFromOutput(metric, results)
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
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_options.output_buildbot_annotations = None
    source_control = source_control_module.DetermineAndCreateSourceControl(
        bisect_options)
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        source_control, bisect_options)
    bisect_instance.opts.target_platform = target_platform
    git_revision = bisect_instance.source_control.ResolveToRevision(
        revision, 'chromium', bisect_perf_module.DEPOT_DEPS_NAME, 100)
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
    options_dict = {
      'debug_ignore_build': True,
      'debug_ignore_sync': True,
      'debug_ignore_perf_test': True,
      'command': 'fake_command',
      'metric': 'fake/metric',
      'good_revision': 280000,
      'bad_revision': 280005,
    }
    bisect_options = bisect_perf_module.BisectOptions.FromDict(options_dict)
    source_control = source_control_module.DetermineAndCreateSourceControl(
        bisect_options)
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        source_control, bisect_options)
    results = bisect_instance.Run(bisect_options.command,
                                  bisect_options.bad_revision,
                                  bisect_options.good_revision,
                                  bisect_options.metric)
    bisect_instance.FormatAndPrintResults(results)


if __name__ == '__main__':
  unittest.main()

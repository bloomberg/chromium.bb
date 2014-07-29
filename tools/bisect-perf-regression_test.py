# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

from auto_bisect import source_control as source_control_module

# Special import necessary because filename contains dash characters.
bisect_perf_module = __import__('bisect-perf-regression')

# Sample output for a performance test used in the results parsing tests below.
RESULTS_OUTPUT = """RESULT write_operations: write_operations= 23089 count
RESULT read_bytes_gpu: read_bytes_gpu= 35201 kb
RESULT write_bytes_gpu: write_bytes_gpu= 542 kb
RESULT telemetry_page_measurement_results: num_failed= 0 count
RESULT telemetry_page_measurement_results: num_errored= 0 count
*RESULT Total: Total_ref= %(value)s
"""


class BisectPerfRegressionTest(unittest.TestCase):
  """Test case for top-level functions in the bisect-perf-regrssion module."""

  def setUp(self):
    """Sets up the test environment before each test method."""
    pass

  def tearDown(self):
    """Cleans up the test environment after each test method."""
    pass

  def testConfidenceScore(self):
    """Tests the confidence calculation."""
    bad_values = [[0, 1], [1, 2]]
    good_values = [[6, 7], [7, 8]]
    # Closest means are mean(1, 2) and mean(6, 7).
    distance = 6.5 - 1.5
    # Standard deviation of [n-1, n, n, n+1] is 0.8165.
    stddev_sum = 0.8165 + 0.8165
    # Expected confidence is an int in the range [0, 100].
    expected_confidence = min(100, int(100 * distance / float(stddev_sum)))
    self.assertEqual(
        expected_confidence,
        bisect_perf_module.ConfidenceScore(bad_values, good_values))

  def testConfidenceScoreZeroConfidence(self):
    """Tests the confidence calculation when it's expected to be 0."""
    bad_values = [[0, 1], [1, 2], [4, 5], [0, 2]]
    good_values = [[4, 5], [6, 7], [7, 8]]
    # Both groups have value lists with means of 4.5, which means distance
    # between groups is zero, and thus confidence is zero.
    self.assertEqual(
        0, bisect_perf_module.ConfidenceScore(bad_values, good_values))

  def testConfidenceScoreMaxConfidence(self):
    """Tests the confidence calculation when it's expected to be 100."""
    bad_values = [[1, 1], [1, 1]]
    good_values = [[1.2, 1.2], [1.2, 1.2]]
    # Standard deviation in both groups is zero, so confidence is 100.
    self.assertEqual(
        100, bisect_perf_module.ConfidenceScore(bad_values, good_values))

  def testParseDEPSStringManually(self):
    """Tests DEPS parsing."""
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        None, bisect_options)

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

    # Should only expect svn/git revisions to come through, and urls to be
    # filtered out.
    expected_vars_dict = {
        'ffmpeg_hash': '@ac4a9f31fe2610bd146857bbd55d7a260003a888',
        'webkit_rev': '@e01ac0a267d1017288bc67fa3c366b10469d8a24',
        'angle_revision': '74697cf2064c0a2c0d7e1b1b28db439286766a05'
    }
    vars_dict = bisect_instance._ParseRevisionsFromDEPSFileManually(
        deps_file_contents)
    self.assertEqual(vars_dict, expected_vars_dict)

  def testTryParseResultValuesFromOutputWithSingleValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= <value>"""
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        None, bisect_options)
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [66.88], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66.88 kb'}))
    self.assertEqual(
        [66.88], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66.88kb'}))
    self.assertEqual(
        [66.88], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': ' 66.88 '}))
    self.assertEqual(
        [-66.88], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': ' -66.88 kb'}))
    self.assertEqual(
        [66], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66 kb'}))
    self.assertEqual(
        [.66], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '.66 kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '. kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': 'aaa kb'}))

  def testTryParseResultValuesFromOutputWithMulitValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= [<value>,<value>, ..]"""
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        None, bisect_options)
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [66.88], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[66.88] kb'}))
    self.assertEqual(
        [66.88, 99.44], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[66.88, 99.44]kb'}))
    self.assertEqual(
        [66.88, 99.44], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[ 66.88, 99.44 ]'}))
    self.assertEqual(
        [-66.88, 99.44], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66.88,99.44] kb'}))
    self.assertEqual(
        [-66, 99], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,99] kb'}))
    self.assertEqual(
        [-66, 99], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,99,] kb'}))
    self.assertEqual(
        [.66, .99], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[.66,.99] kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[] kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,abc] kb'}))

  def testTryParseResultValuesFromOutputWithMeanStd(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= {<mean, std}"""
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        None, bisect_options)
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [33.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22, 3.6} kb'}))
    self.assertEqual(
        [33.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22,3.6}kb'}))
    self.assertEqual(
        [33.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22,3.6} kb'}))
    self.assertEqual(
        [33.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{ 33.22,3.6 }kb'}))
    self.assertEqual(
        [-33.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{-33.22,3.6}kb'}))
    self.assertEqual(
        [22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{22,6}kb'}))
    self.assertEqual(
        [.22], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{.22,6}kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{.22,6, 44}kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{}kb'}))
    self.assertEqual(
        [], bisect_instance.TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{XYZ}kb'}))

  def testGetCompatibleCommand(self):
    bisect_options = bisect_perf_module.BisectOptions()
    bisect_options.output_buildbot_annotations = None
    source_control = source_control_module.DetermineAndCreateSourceControl(
        bisect_options)
    bisect_instance = bisect_perf_module.BisectPerformanceMetrics(
        source_control, bisect_options)
    bisect_instance.opts.target_platform = 'android'
    depot = 'chromium'
    # android-chrome-shell -> android-chromium-testshell
    revision = 274857
    git_revision = bisect_instance.source_control.ResolveToRevision(
        revision, 'chromium', bisect_perf_module.DEPOT_DEPS_NAME, 100)
    command = ('tools/perf/run_benchmark -v '
               '--browser=android-chrome-shell page_cycler.intl_ja_zh')
    expected_command = ('tools/perf/run_benchmark -v --browser='
                        'android-chromium-testshell page_cycler.intl_ja_zh')
    self.assertEqual(
        bisect_instance.GetCompatibleCommand(command, git_revision, depot),
        expected_command)

    # android-chromium-testshell -> android-chromium-testshell
    revision = 274858
    git_revision = bisect_instance.source_control.ResolveToRevision(
        revision, 'chromium', bisect_perf_module.DEPOT_DEPS_NAME, 100)
    command = ('tools/perf/run_benchmark -v '
               '--browser=android-chromium-testshell page_cycler.intl_ja_zh')
    expected_command = ('tools/perf/run_benchmark -v --browser='
                        'android-chromium-testshell page_cycler.intl_ja_zh')
    self.assertEqual(
        bisect_instance.GetCompatibleCommand(command, git_revision, depot),
        expected_command)

    # android-chromium-testshell -> android-chrome-shell
    revision = 276628
    git_revision = bisect_instance.source_control.ResolveToRevision(
        revision, 'chromium', bisect_perf_module.DEPOT_DEPS_NAME, 100)
    command = ('tools/perf/run_benchmark -v '
               '--browser=android-chromium-testshell page_cycler.intl_ja_zh')
    expected_command = ('tools/perf/run_benchmark -v --browser='
                        'android-chrome-shell page_cycler.intl_ja_zh')
    self.assertEqual(
        bisect_instance.GetCompatibleCommand(command, git_revision, depot),
        expected_command)
    # android-chrome-shell -> android-chrome-shell
    command = ('tools/perf/run_benchmark -v '
               '--browser=android-chrome-shell page_cycler.intl_ja_zh')
    expected_command = ('tools/perf/run_benchmark -v --browser='
                        'android-chrome-shell page_cycler.intl_ja_zh')
    self.assertEqual(
        bisect_instance.GetCompatibleCommand(command, git_revision, depot),
        expected_command)
    # Not android platform
    bisect_instance.opts.target_platform = 'chromium'
    command = ('tools/perf/run_benchmark -v '
               '--browser=release page_cycler.intl_ja_zh')
    expected_command = ('tools/perf/run_benchmark -v --browser='
                        'release page_cycler.intl_ja_zh')
    self.assertEqual(
        bisect_instance.GetCompatibleCommand(command, git_revision, depot),
        expected_command)


if __name__ == '__main__':
  unittest.main()

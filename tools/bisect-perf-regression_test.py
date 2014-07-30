# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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


# Some private methods of the bisect-perf-regression module are tested below.
# pylint: disable=W0212
class BisectPerfRegressionTest(unittest.TestCase):
  """Test case for top-level functions in the bisect-perf-regrssion module."""

  def setUp(self):
    """Sets up the test environment before each test method."""
    pass

  def tearDown(self):
    """Cleans up the test environment after each test method."""
    pass

  def testConfidenceScoreHigh(self):
    """Tests the confidence calculation."""
    bad_values = [[0, 1, 1], [1, 2, 2]]
    good_values = [[1, 2, 2], [3, 3, 4]]
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(95.0, confidence)

  def testConfidenceScoreNotSoHigh(self):
    """Tests the confidence calculation."""
    bad_values = [[0, 1, 1], [1, 2, 2]]
    good_values = [[1, 1, 1], [3, 3, 4]]
    # The good and bad groups are closer together than in the above test,
    # so the confidence that they're different is a little lower.
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(80.0, confidence)

  def testConfidenceScoreZero(self):
    """Tests the confidence calculation when it's expected to be 0."""
    bad_values = [[4, 5], [7, 6], [8, 7]]
    good_values = [[8, 7], [6, 7], [5, 4]]
    # The good and bad sets contain the same values, so the confidence that
    # they're different should be zero.
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(0.0, confidence)

  def testConfidenceScoreVeryHigh(self):
    """Tests the confidence calculation when it's expected to be high."""
    bad_values = [[1, 1], [1, 1]]
    good_values = [[1.2, 1.2], [1.2, 1.2]]
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(99.9, confidence)

  def testConfidenceScoreImbalance(self):
    """Tests the confidence calculation one set of numbers is small."""
    bad_values = [[1.1, 1.2], [1.1, 1.2], [1.0, 1.3], [1.2, 1.3]]
    good_values = [[1.4]]
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(80.0, confidence)

  def testConfidenceScoreImbalance(self):
    """Tests the confidence calculation one set of numbers is empty."""
    bad_values = [[1.1, 1.2], [1.1, 1.2], [1.0, 1.3], [1.2, 1.3]]
    good_values = []
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(0.0, confidence)

  def testConfidenceScoreFunctionalTestResultsInconsistent(self):
    """Tests the confidence calculation when the numbers are just 0 and 1."""
    bad_values = [[1], [1], [0], [1], [1], [1], [0], [1]]
    good_values = [[0], [0], [1], [0], [1], [0]]
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(80.0, confidence)

  def testConfidenceScoreFunctionalTestResultsConsistent(self):
    """Tests the confidence calculation when the numbers are 0 and 1."""
    bad_values = [[1], [1], [1], [1], [1], [1], [1], [1]]
    good_values = [[0], [0], [0], [0], [0], [0]]
    confidence = bisect_perf_module.ConfidenceScore(bad_values, good_values)
    self.assertEqual(99.9, confidence)

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

    # Should only expect svn/git revisions to come through, and urls should be
    # filtered out.
    expected_vars_dict = {
        'ffmpeg_hash': '@ac4a9f31fe2610bd146857bbd55d7a260003a888',
        'webkit_rev': '@e01ac0a267d1017288bc67fa3c366b10469d8a24',
        'angle_revision': '74697cf2064c0a2c0d7e1b1b28db439286766a05'
    }
    vars_dict = bisect_perf_module._ParseRevisionsFromDEPSFileManually(
        deps_file_contents)
    self.assertEqual(vars_dict, expected_vars_dict)

  def testTryParseResultValuesFromOutputWithSingleValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= <value>"""
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [66.88], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66.88 kb'}))
    self.assertEqual(
        [66.88], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66.88kb'}))
    self.assertEqual(
        [66.88], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': ' 66.88 '}))
    self.assertEqual(
        [-66.88], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': ' -66.88 kb'}))
    self.assertEqual(
        [66], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '66 kb'}))
    self.assertEqual(
        [.66], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '.66 kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '. kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': 'aaa kb'}))

  def testTryParseResultValuesFromOutputWithMulitValue(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= [<value>,<value>, ..]"""
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [66.88], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[66.88] kb'}))
    self.assertEqual(
        [66.88, 99.44], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[66.88, 99.44]kb'}))
    self.assertEqual(
        [66.88, 99.44], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[ 66.88, 99.44 ]'}))
    self.assertEqual(
        [-66.88, 99.44], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66.88,99.44] kb'}))
    self.assertEqual(
        [-66, 99], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,99] kb'}))
    self.assertEqual(
        [-66, 99], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,99,] kb'}))
    self.assertEqual(
        [.66, .99], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[.66,.99] kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[] kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '[-66,abc] kb'}))

  def testTryParseResultValuesFromOutputWithMeanStd(self):
    """Tests result pattern <*>RESULT <graph>: <trace>= {<mean, std}"""
    metrics = ['Total', 'Total_ref']
    self.assertEqual(
        [33.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22, 3.6} kb'}))
    self.assertEqual(
        [33.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22,3.6}kb'}))
    self.assertEqual(
        [33.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{33.22,3.6} kb'}))
    self.assertEqual(
        [33.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{ 33.22,3.6 }kb'}))
    self.assertEqual(
        [-33.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{-33.22,3.6}kb'}))
    self.assertEqual(
        [22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{22,6}kb'}))
    self.assertEqual(
        [.22], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{.22,6}kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{.22,6, 44}kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
            metrics, RESULTS_OUTPUT % {'value': '{}kb'}))
    self.assertEqual(
        [], bisect_perf_module._TryParseResultValuesFromOutput(
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

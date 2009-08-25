# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys

from layout_package import path_utils
from layout_package import test_failures

sys.path.append(path_utils.PathFromBase('third_party'))
import simplejson

class JSONResultsGenerator:

  MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 100
  # Min time (seconds) that will be added to the JSON.
  MIN_TIME = 1
  JSON_PREFIX = "ADD_RESULTS("
  JSON_SUFFIX = ");"
  WEBKIT_PATH = "WebKit"
  LAYOUT_TESTS_PATH = "layout_tests"
  PASS_RESULT = "P"
  NO_DATA_RESULT = "N"

  def __init__(self, failures, individual_test_timings, builder_name,
      build_number, results_file_path, all_tests):
    """
    failures: Map of test name to list of failures.
    individual_test_times: Map of test name to a tuple containing the
        test_run-time.
    builder_name: The name of the builder the tests are being run on.
    build_number: The build number for this run.
    results_file_path: Absolute path to the results json file.
    all_tests: List of all the tests that were run.
    """
    # Make sure all test paths are relative to the layout test root directory.
    self._failures = {}
    for test in failures:
      test_path = self._GetPathRelativeToLayoutTestRoot(test)
      self._failures[test_path] = failures[test]

    self._all_tests = [self._GetPathRelativeToLayoutTestRoot(test)
        for test in all_tests]

    self._test_timings = {}
    for test_tuple in individual_test_timings:
      test_path = self._GetPathRelativeToLayoutTestRoot(test_tuple.filename)
      self._test_timings[test_path] = test_tuple.test_run_time

    self._builder_name = builder_name
    self._build_number = build_number
    self._results_file_path = results_file_path

  def _GetPathRelativeToLayoutTestRoot(self, test):
    """Returns the path of the test relative to the layout test root.
    Example paths are
      src/third_party/WebKit/LayoutTests/fast/forms/foo.html
      src/webkit/data/layout_tests/pending/fast/forms/foo.html
    We would return the following:
      LayoutTests/fast/forms/foo.html
      pending/fast/forms/foo.html
    """
    index = test.find(self.WEBKIT_PATH)
    if index is not -1:
      index += len(self.WEBKIT_PATH)
    else:
      index = test.find(self.LAYOUT_TESTS_PATH)
      if index is not -1:
        index += len(self.LAYOUT_TESTS_PATH)

    if index is -1:
      # Already a relative path.
      relativePath = test
    else:
      relativePath = test[index + 1:]

    # Make sure all paths are unix-style.
    return relativePath.replace('\\', '/')

  def GetJSON(self):
    """Gets the results for the results.json file."""
    failures_for_json = {}
    for test in self._failures:
      failures_for_json[test] = ResultAndTime(test, self._all_tests)
      failures_for_json[test].result = self._GetResultsCharForFailure(test)

    for test in self._test_timings:
      if not test in failures_for_json:
        failures_for_json[test] = ResultAndTime(test, self._all_tests)
      # Floor for now to get time in seconds.
      # TODO(ojan): As we make tests faster, reduce to tenth of a second
      # granularity.
      failures_for_json[test].time = int(self._test_timings[test])

    # If results file exists, read it out, put new info in it.
    if os.path.exists(self._results_file_path):
      old_results_file = open(self._results_file_path, "r")
      old_results = old_results_file.read()
      # Strip the prefix and suffix so we can get the actual JSON object.
      old_results = old_results[
          len(self.JSON_PREFIX) : len(old_results) - len(self.JSON_SUFFIX)]

      try:
        results_json = simplejson.loads(old_results)
      except:
        # The JSON file is not valid JSON. Just clobber the results.
        results_json = {}

      if self._builder_name not in results_json:
        logging.error("Builder name (%s) is not in the results.json file." %
            self._builder_name);
    else:
      # TODO(ojan): If the build output directory gets clobbered, we should
      # grab this file off wherever it's archived to. Maybe we should always
      # just grab it from wherever it's archived to.
      results_json = {}

    if self._builder_name not in results_json:
      results_json[self._builder_name] = self._CreateResultsForBuilderJSON()

    tests = results_json[self._builder_name]["tests"]
    all_failing_tests = set(self._failures.iterkeys())
    all_failing_tests.update(tests.iterkeys())

    for test in all_failing_tests:
      if test in failures_for_json:
        result_and_time = failures_for_json[test]
      else:
        result_and_time = ResultAndTime(test, self._all_tests)

      if test not in tests:
        tests[test] = self._CreateResultsAndTimesJSON()

      thisTest = tests[test]
      thisTest["results"] = result_and_time.result + thisTest["results"]
      thisTest["times"].insert(0, result_and_time.time)

      self._NormalizeResultsJSON(thisTest, test, tests)

    results_json[self._builder_name]["buildNumbers"].insert(0,
        self._build_number)

    # Specify separators in order to get compact encoding.
    results_str = simplejson.dumps(results_json, separators=(',', ':'))
    return self.JSON_PREFIX + results_str + self.JSON_SUFFIX

  def _CreateResultsAndTimesJSON(self):
    results_and_times = {}
    results_and_times["results"] = ""
    results_and_times["times"] = []
    return results_and_times

  def _CreateResultsForBuilderJSON(self):
    results_for_builder = {}
    results_for_builder['buildNumbers'] = []
    results_for_builder['tests'] = {}
    return results_for_builder

  def _GetResultsCharForFailure(self, test):
    """Returns the worst failure from the list of failures for this test
    since we can only show one failure per run for each test on the dashboard.
    """
    failures = [failure.__class__ for failure in self._failures[test]]

    if test_failures.FailureCrash in failures:
      return "C"
    elif test_failures.FailureTimeout in failures:
      return "T"
    elif test_failures.FailureImageHashMismatch in failures:
      return "I"
    elif test_failures.FailureSimplifiedTextMismatch in failures:
      return "S"
    elif test_failures.FailureTextMismatch in failures:
      return "F"
    else:
      return "O"

  def _NormalizeResultsJSON(self, test, test_path, tests):
    """ Prune tests where all runs pass or tests that no longer exist and
    truncate all results to maxNumberOfBuilds and pad results that don't
    have encough runs for maxNumberOfBuilds.

    Args:
      test: ResultsAndTimes object for this test.
      test_path: Path to the test.
      tests: The JSON object with all the test results for this builder.
    """
    results = test["results"]
    num_results = len(results)
    times = test["times"]

    if num_results != len(times):
      logging.error("Test has different number of build times versus results")
      times = []
      results = ""
      num_results = 0

    # Truncate or right-pad so there are exactly maxNumberOfBuilds results.
    if num_results > self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
      results = results[:self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG]
      times = times[:self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG]
    elif num_results < self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
      num_to_pad = self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG - num_results
      results = results + num_to_pad * self.NO_DATA_RESULT
      times.extend(num_to_pad * [0])

    test["results"] = results
    test["times"] = times

    # If the test has all passes or all no-data (that wasn't just padded in) and
    # times that take less than a second, remove it from the results to reduce
    # noise and filesize.
    if (max(times) >= self.MIN_TIME and num_results and
        (results == self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG *
             self.PASS_RESULT or
         results == self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG *
             self.NO_DATA_RESULT)):
      del tests[test_path]

    # Remove tests that don't exist anymore.
    full_path = os.path.join(path_utils.LayoutTestsDir(test_path), test_path)
    full_path = os.path.normpath(full_path)
    if not os.path.exists(full_path):
      del tests[test_path]

class ResultAndTime:
  """A holder for a single result and runtime for a test."""
  def __init__(self, test, all_tests):
    self.time = 0
    # If the test was run, then we don't want to default the result to nodata.
    if test in all_tests:
      self.result = JSONResultsGenerator.PASS_RESULT
    else:
      self.result = JSONResultsGenerator.NO_DATA_RESULT

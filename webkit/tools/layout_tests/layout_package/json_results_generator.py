# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

from layout_package import path_utils
from layout_package import test_failures

class ResultAndTime:
  """A holder for a single result and runtime for a test."""
  time = 0
  result = "N"

class JSONResultsGenerator:

  MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 100
  # Min time (seconds) that will be added to the JSON.
  MIN_TIME = 1
  JSON_PREFIX = "ADD_RESULTS("
  JSON_SUFFIX = ");"

  def __init__(self, failures, individual_test_timings, builder_name,
      build_number, results_file_path):
    """
    failures: Map of test name to list of failures.
    individual_test_times: Map of test name to a tuple containing the
        test_run-time.
    builder_name: The name of the builder the tests are being run on.
    build_number: The build number for this run.
    results_file_path: Absolute path to the results json file.
    """
    self._failures = failures
    self._test_timings = individual_test_timings
    self._builder_name = builder_name
    self._build_number = build_number
    self._results_file_path = results_file_path

  def GetJSON(self):
    """Gets the results for the results.json file."""
    failures_for_json = {}
    for test in self._failures:
      failures_for_json[test] = ResultAndTime()
      # TODO(ojan): get relative path here
      failures_for_json[test].result = self._GetResultsCharForFailure(test)

    for test_tuple in self._test_timings:
      test = test_tuple.filename
      if not test in failures_for_json:
        failures_for_json[test] = ResultAndTime()
      # Floor for now to get time in seconds.
      # TODO(ojan): As we make tests faster, reduce to tenth of a second
      # granularity.
      failures_for_json[test].time = int(test_tuple.test_run_time)

    # If results file exists, read it out, put new info in it.
    if os.path.exists(self._results_file_path):
      old_results_file = open(self._results_file_path, "r")
      old_results = old_results_file.read()
      # Strip the prefix and suffix so we can get the actual JSON object.
      old_results = old_results[
          len(self.JSON_PREFIX) : len(old_results) - len(self.JSON_SUFFIX)]
      results_json = eval(old_results)

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
        result_and_time = ResultAndTime()

      if test not in tests:
        tests[test] = self._CreateResultsAndTimesJSON()

      thisTest = tests[test]
      thisTest["results"] = result_and_time.result + thisTest["results"]
      thisTest["times"].insert(0, result_and_time.time)

      self._NormalizeResultsJSON(thisTest, test, tests)

    results_json[self._builder_name]["buildNumbers"].insert(0,
        self._build_number)

    # Generate the JSON and strip whitespace to keep filesize down.
    # TODO(ojan): Generate the JSON using a JSON library should someone ever
    # add a non-primitive type to results_json.
    results_str = self.JSON_PREFIX + repr(results_json) + self.JSON_SUFFIX
    return re.sub(r'\s+', '', results_str)

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
      results = results + num_to_pad * 'N'
      times.extend(num_to_pad * [0])

    test["results"] = results
    test["times"] = times

    # If the test has all passes or all no-data (that wasn't just padded in) and
    # times that take less than a second, remove it from the results to reduce
    # noise and filesize.
    if (max(times) >= self.MIN_TIME and num_results and
        (results == self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG * 'P' or
         results == self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG * 'N')):
      del tests[test_path]

    # Remove tests that don't exist anymore.
    full_path = os.path.join(path_utils.LayoutTestsDir(test_path), test_path)
    full_path = os.path.normpath(full_path)
    if not os.path.exists(full_path):
      del tests[test_path]

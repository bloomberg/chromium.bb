# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import sys
import time
import urllib2
import xml.dom.minidom

from layout_package import path_utils
from layout_package import test_failures

sys.path.append(path_utils.PathFromBase('third_party'))
import simplejson

class JSONResultsGenerator:

  MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 750
  # Min time (seconds) that will be added to the JSON.
  MIN_TIME = 1
  JSON_PREFIX = "ADD_RESULTS("
  JSON_SUFFIX = ");"
  WEBKIT_PATH = "WebKit"
  LAYOUT_TESTS_PATH = "layout_tests"
  PASS_RESULT = "P"
  SKIP_RESULT = "X"
  NO_DATA_RESULT = "N"
  VERSION = 1
  VERSION_KEY = "version"
  RESULTS = "results"
  TIMES = "times"
  BUILD_NUMBERS = "buildNumbers"
  WEBKIT_SVN = "webkitRevision"
  CHROME_SVN = "chromeRevision"
  TIME = "secondsSinceEpoch"
  TESTS = "tests"
  WONTFIX = "wontfixCounts"
  DEFERRED = "deferredCounts"
  FIXABLE = "fixableCounts"
  ALL_FIXABLE_COUNT = "allFixableCount"
  FIXABLE_COUNT = "fixableCount"
  """
    C = CRASH
    T = TIMEOUT
    I = IMAGE
    F = TEXT
    O = OTHER
    Z = TEXT+IMAGE
  """
  FAILURE_CHARS = ("C", "T", "I", "F", "O", "Z")
  BUILDER_BASE_URL = "http://build.chromium.org/buildbot/layout_test_results/"
  RESULTS_FILENAME = "results.json"

  def __init__(self, options, failures, individual_test_timings,
      results_file_base_path, all_tests, result_summary):
    """Modifies the results.json file. Grabs it off the archive directory if it
    is not found locally.

    Args
      options: a dictionary of command line options
      failures: Map of test name to list of failures.
      individual_test_times: Map of test name to a tuple containing the
          test_run-time.
      results_file_base_path: Absolute path to the directory containing the
          results json file.
      all_tests: List of all the tests that were run.
      result_summary: ResultsSummary object containing failure counts for
          different groups of tests.
    """
    # Make sure all test paths are relative to the layout test root directory.
    self._failures = {}
    for test in failures:
      test_path = self._GetPathRelativeToLayoutTestRoot(test)
      self._failures[test_path] = failures[test]

    self._all_tests = [self._GetPathRelativeToLayoutTestRoot(test)
        for test in all_tests]

    self._result_summary = result_summary

    self._test_timings = {}
    for test_tuple in individual_test_timings:
      test_path = self._GetPathRelativeToLayoutTestRoot(test_tuple.filename)
      self._test_timings[test_path] = test_tuple.test_run_time

    self._options = options
    self._results_file_path = os.path.join(results_file_base_path,
        self.RESULTS_FILENAME)

    json = self._GetJSON()
    if json:
      results_file = open(self._results_file_path, "w")
      results_file.write(json)
      results_file.close()

  def _GetSVNRevision(self, in_directory=None):
    """Returns the svn revision for the given directory.

    Args:
      in_directory: The directory where svn is to be run.
    """
    output = subprocess.Popen(["svn", "info", "--xml"],
                              cwd=in_directory,
                              shell=(sys.platform == 'win32'),
                              stdout=subprocess.PIPE).communicate()[0]
    try :
      dom = xml.dom.minidom.parseString(output)
      return dom.getElementsByTagName('entry')[0].getAttribute('revision');
    except xml.parsers.expat.ExpatError:
      return ""

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

  def _GetJSON(self):
    """Gets the results for the results.json file."""
    failures_for_json = {}
    for test in self._failures:
      failures_for_json[test] = ResultAndTime(test, self._all_tests)
      failures_for_json[test].result = self._GetResultsCharForTest(test)

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
    else:
      results_file_url = (self.BUILDER_BASE_URL + self._options.build_name +
          "/" + self.RESULTS_FILENAME)
      logging.error("Local results.json file does not exist. Grabbing it off "
          "the archive at " + results_file_url)

      error = None
      old_results = None
      try:
        results_file = urllib2.urlopen(results_file_url)
        info = results_file.info()
        old_results = results_file.read()
      except urllib2.HTTPError, http_error:
        # A non-4xx status code means the bot is hosed for some reason and we
        # can't grab the results.json file off of it.
        if (http_error.code < 400 and http_error.code >= 500):
          error = http_error
      except urllib2.URLError, url_error:
        error = url_error

      if error:
        # If there was a URL/HTTPError don't write a results.json
        # file at all as it would lose all the information on the bot.
        logging.error("Archive directory is inaccessible. Not modifying or "
            "clobbering the results.json file: " + str(error))
        return None

    builder_name = self._options.builder_name
    if old_results:
      # Strip the prefix and suffix so we can get the actual JSON object.
      old_results = old_results[
          len(self.JSON_PREFIX) : len(old_results) - len(self.JSON_SUFFIX)]

      try:
        results_json = simplejson.loads(old_results)
      except:
        logging.error("results.json was not valid JSON. Clobbering.")
        # The JSON file is not valid JSON. Just clobber the results.
        results_json = {}

      if builder_name not in results_json:
        logging.error("Builder name (%s) is not in the results.json file." %
            builder_name);
    else:
      results_json = {}

    self._ConvertJSONToCurrentVersion(results_json)

    if builder_name not in results_json:
      results_json[builder_name] = self._CreateResultsForBuilderJSON()

    results_for_builder = results_json[builder_name]
    tests = results_for_builder[self.TESTS]
    all_failing_tests = set(self._failures.iterkeys())
    all_failing_tests.update(tests.iterkeys())

    self._InsertItemIntoRawList(results_for_builder,
        self._options.build_number, self.BUILD_NUMBERS)

    path_to_webkit = path_utils.PathFromBase('third_party', 'WebKit', 'WebCore')
    self._InsertItemIntoRawList(results_for_builder,
        self._GetSVNRevision(path_to_webkit),
        self.WEBKIT_SVN)

    path_to_chrome_base = path_utils.PathFromBase()
    self._InsertItemIntoRawList(results_for_builder,
        self._GetSVNRevision(path_to_chrome_base),
        self.CHROME_SVN)

    self._InsertItemIntoRawList(results_for_builder,
        int(time.time()),
        self.TIME)

    self._InsertFailureSummaries(results_for_builder)

    for test in all_failing_tests:
      if test in failures_for_json:
        result_and_time = failures_for_json[test]
      else:
        result_and_time = ResultAndTime(test, self._all_tests)

      if test not in tests:
        tests[test] = self._CreateResultsAndTimesJSON()

      thisTest = tests[test]
      self._InsertItemRunLengthEncoded(result_and_time.result,
          thisTest[self.RESULTS])
      self._InsertItemRunLengthEncoded(result_and_time.time,
          thisTest[self.TIMES])
      self._NormalizeResultsJSON(thisTest, test, tests)

    # Specify separators in order to get compact encoding.
    results_str = simplejson.dumps(results_json, separators=(',', ':'))
    return self.JSON_PREFIX + results_str + self.JSON_SUFFIX

  def _InsertFailureSummaries(self, results_for_builder):
    """Inserts aggregate pass/failure statistics into the JSON.

    Args:
      results_for_builder: Dictionary containing the test results for a single
          builder.
    """
    self._InsertItemIntoRawList(results_for_builder,
        self._result_summary.fixable_count,
        self.FIXABLE_COUNT)
    self._InsertItemIntoRawList(results_for_builder,
        self._result_summary.all_fixable_count,
        self.ALL_FIXABLE_COUNT)

    self._InsertItemIntoRawList(results_for_builder,
        self._GetFailureSummaryEntry(self._result_summary.deferred),
        self.DEFERRED)
    self._InsertItemIntoRawList(results_for_builder,
        self._GetFailureSummaryEntry(self._result_summary.wontfix),
        self.WONTFIX)
    self._InsertItemIntoRawList(results_for_builder,
        self._GetFailureSummaryEntry(self._result_summary.fixable),
        self.FIXABLE)

  def _GetFailureSummaryEntry(self, result_summary_entry):
    """Creates a summary object to insert into the JSON.

    Args:
      result_summary_entry: ResultSummaryEntry for a group of tests
          (e.g. deferred tests).
    """
    entry = {}
    entry[self.SKIP_RESULT] = result_summary_entry.skip_count
    entry[self.PASS_RESULT] = result_summary_entry.pass_count
    for char in self.FAILURE_CHARS:
      # There can be multiple failures that map to "O", so keep existing entry
      # values if they already exist.
      count = entry.get(char, 0)

      for failure in result_summary_entry.failure_counts:
        if char == self._GetResultsCharForFailure([failure]):
          count = result_summary_entry.failure_counts[failure]
      entry[char] = count
    return entry

  def _InsertItemIntoRawList(self, results_for_builder, item, key):
    """Inserts the item into the list with the given key in the results for
    this builder. Creates the list if no such list exists.

    Args:
      results_for_builder: Dictionary containing the test results for a single
          builder.
      item: Number or string to insert into the list.
      key: Key in results_for_builder for the list to insert into.
    """
    if key in results_for_builder:
      raw_list = results_for_builder[key]
    else:
      raw_list = []

    raw_list.insert(0, item)
    raw_list = raw_list[:self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG]
    results_for_builder[key] = raw_list

  def _InsertItemRunLengthEncoded(self, item, encoded_results):
    """Inserts the item into the run-length encoded results.

    Args:
      item: String or number to insert.
      encoded_results: run-length encoded results. An array of arrays, e.g.
          [[3,'A'],[1,'Q']] encodes AAAQ.
    """
    if len(encoded_results) and item == encoded_results[0][1]:
      num_results = encoded_results[0][0]
      if num_results <= self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
        encoded_results[0][0] = num_results + 1
    else:
      # Use a list instead of a class for the run-length encoding since we
      # want the serialized form to be concise.
      encoded_results.insert(0, [1, item])

  def _ConvertJSONToCurrentVersion(self, results_json):
    """If the JSON does not match the current version, converts it to the
    current version and adds in the new version number.
    """
    if (self.VERSION_KEY in results_json and
        results_json[self.VERSION_KEY] == self.VERSION):
      return

    results_json[self.VERSION_KEY] = self.VERSION

  def _CreateResultsAndTimesJSON(self):
    results_and_times = {}
    results_and_times[self.RESULTS] = []
    results_and_times[self.TIMES] = []
    return results_and_times

  def _CreateResultsForBuilderJSON(self):
    results_for_builder = {}
    results_for_builder[self.TESTS] = {}
    return results_for_builder

  def _GetResultsCharForTest(self, test):
    """Returns the worst failure from the list of failures for this test
    since we can only show one failure per run for each test on the dashboard.
    """
    failures = [failure.__class__ for failure in self._failures[test]]
    return self._GetResultsCharForFailure(failures)

  def _GetResultsCharForFailure(self, failures):
    """Returns the worst failure from the list of failures
    since we can only show one failure per run for each test on the dashboard.
    """
    has_text_failure = test_failures.FailureTextMismatch in failures

    if test_failures.FailureCrash in failures:
      return "C"
    elif test_failures.FailureTimeout in failures:
      return "T"
    elif test_failures.FailureImageHashMismatch in failures:
      if has_text_failure:
        return "Z"
      return "I"
    elif has_text_failure:
      return "F"
    else:
      return "O"

  def _RemoveItemsOverMaxNumberOfBuilds(self, encoded_list):
    """Removes items from the run-length encoded list after the final item that
    exceeds the max number of builds to track.

    Args:
      encoded_results: run-length encoded results. An array of arrays, e.g.
          [[3,'A'],[1,'Q']] encodes AAAQ.
    """
    num_builds = 0
    index = 0
    for result in encoded_list:
      num_builds = num_builds + result[0]
      index = index + 1
      if num_builds > self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
        return encoded_list[:index]
    return encoded_list

  def _NormalizeResultsJSON(self, test, test_path, tests):
    """ Prune tests where all runs pass or tests that no longer exist and
    truncate all results to maxNumberOfBuilds.

    Args:
      test: ResultsAndTimes object for this test.
      test_path: Path to the test.
      tests: The JSON object with all the test results for this builder.
    """
    test[self.RESULTS] = self._RemoveItemsOverMaxNumberOfBuilds(
        test[self.RESULTS])
    test[self.TIMES] = self._RemoveItemsOverMaxNumberOfBuilds(test[self.TIMES])

    is_all_pass = self._IsResultsAllOfType(test[self.RESULTS], self.PASS_RESULT)
    is_all_no_data = self._IsResultsAllOfType(test[self.RESULTS],
        self.NO_DATA_RESULT)
    max_time = max(test[self.TIMES], lambda x, y : cmp(x[1], y[1]))

    # Remove all passes/no-data from the results to reduce noise and filesize.
    # If a test passes every run, but takes > MIN_TIME to run, don't throw away
    # the data.
    if is_all_no_data or (is_all_pass and max_time <= self.MIN_TIME):
      del tests[test_path]
    else:
      # Remove tests that don't exist anymore.
      full_path = os.path.join(path_utils.LayoutTestsDir(test_path), test_path)
      full_path = os.path.normpath(full_path)
      if not os.path.exists(full_path):
        del tests[test_path]

  def _IsResultsAllOfType(self, results, type):
    """Returns whether all teh results are of the given type (e.g. all passes).
    """
    return len(results) == 1 and results[0][1] == type

class ResultAndTime:
  """A holder for a single result and runtime for a test."""
  def __init__(self, test, all_tests):
    self.time = 0
    # If the test was run, then we don't want to default the result to nodata.
    if test in all_tests:
      self.result = JSONResultsGenerator.PASS_RESULT
    else:
      self.result = JSONResultsGenerator.NO_DATA_RESULT

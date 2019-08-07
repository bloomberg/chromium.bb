# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import copy
import re


# These labels should match the ones output by gtest's JSON.
TEST_UNKNOWN_LABEL = 'UNKNOWN'
TEST_SUCCESS_LABEL = 'SUCCESS'
TEST_FAILURE_LABEL = 'FAILURE'
TEST_TIMEOUT_LABEL = 'TIMEOUT'
TEST_WARNING_LABEL = 'WARNING'


class GTestResult(object):
  """A result of gtest.

  Properties:
    command: The command argv.
    crashed: Whether or not the test crashed.
    crashed_test: The name of the test during which execution crashed, or
      None if a particular test didn't crash.
    failed_tests: A dict mapping the names of failed tests to a list of
      lines of output from those tests.
    flaked_tests: A dict mapping the names of failed flaky tests to a list
      of lines of output from those tests.
    passed_tests: A list of passed tests.
    perf_links: A dict mapping the names of perf data points collected
      to links to view those graphs.
    return_code: The return code of the command.
    success: Whether or not this run of the command was considered a
      successful GTest execution.
  """
  @property
  def crashed(self):
    return self._crashed

  @property
  def crashed_test(self):
    return self._crashed_test

  @property
  def command(self):
    return self._command

  @property
  def failed_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._failed_tests)
    return self._failed_tests

  @property
  def flaked_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._flaked_tests)
    return self._flaked_tests

  @property
  def passed_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._passed_tests)
    return self._passed_tests

  @property
  def perf_links(self):
    if self.__finalized:
      return copy.deepcopy(self._perf_links)
    return self._perf_links

  @property
  def return_code(self):
    return self._return_code

  @property
  def success(self):
    return self._success

  def __init__(self, command):
    if not isinstance(command, collections.Iterable):
      raise ValueError('Expected an iterable of command arguments.', command)

    if not command:
      raise ValueError('Expected a non-empty command.', command)

    self._command = tuple(command)
    self._crashed = False
    self._crashed_test = None
    self._failed_tests = collections.OrderedDict()
    self._flaked_tests = collections.OrderedDict()
    self._passed_tests = []
    self._perf_links = collections.OrderedDict()
    self._return_code = None
    self._success = None
    self.__finalized = False

  def finalize(self, return_code, success):
    self._return_code = return_code
    self._success = success

    # If the test was not considered to be a GTest success, but had no
    # failing tests, conclude that it must have crashed.
    if not self._success and not self._failed_tests and not self._flaked_tests:
      self._crashed = True

    # At most one test can crash the entire app in a given parsing.
    for test, log_lines in self._failed_tests.iteritems():
      # A test with no output would have crashed. No output is replaced
      # by the GTestLogParser by a sentence indicating non-completion.
      if 'Did not complete.' in log_lines:
        self._crashed = True
        self._crashed_test = test

    # A test marked as flaky may also have crashed the app.
    for test, log_lines in self._flaked_tests.iteritems():
      if 'Did not complete.' in log_lines:
        self._crashed = True
        self._crashed_test = test

    self.__finalized = True


class GTestLogParser(object):
  """This helper class process GTest test output."""

  def __init__(self):
    # State tracking for log parsing
    self.completed = False
    self._current_test = ''
    self._failure_description = []
    self._parsing_failures = False

    # Line number currently being processed.
    self._line_number = 0

    # List of parsing errors, as human-readable strings.
    self._internal_error_lines = []

    # Tests are stored here as 'test.name': (status, [description]).
    # The status should be one of ('started', 'OK', 'failed', 'timeout',
    # 'warning'). Warning indicates that a test did not pass when run in
    # parallel with other tests but passed when run alone. The description is
    # a list of lines detailing the test's error, as reported in the log.
    self._test_status = {}

    # This may be either text or a number. It will be used in the phrase
    # '%s disabled' or '%s flaky' on the waterfall display.
    self._disabled_tests = 0
    self._flaky_tests = 0

    # Regular expressions for parsing GTest logs. Test names look like
    # "x.y", with 0 or more "w/" prefixes and 0 or more "/z" suffixes.
    # e.g.:
    #   SomeName/SomeTestCase.SomeTest/1
    #   SomeName/SomeTestCase/1.SomeTest
    #   SomeName/SomeTestCase/1.SomeTest/SomeModifider
    test_name_regexp = r'((\w+/)*\w+\.\w+(/\w+)*)'

    self._master_name_re = re.compile(r'\[Running for master: "([^"]*)"')
    self.master_name = ''

    self._test_name = re.compile(test_name_regexp)
    self._test_start = re.compile(r'\[\s+RUN\s+\] ' + test_name_regexp)
    self._test_ok = re.compile(r'\[\s+OK\s+\] ' + test_name_regexp)
    self._test_fail = re.compile(r'\[\s+FAILED\s+\] ' + test_name_regexp)
    self._test_passed = re.compile(r'\[\s+PASSED\s+\] \d+ tests?.')
    self._run_test_cases_line = re.compile(
        r'\[\s*\d+\/\d+\]\s+[0-9\.]+s ' + test_name_regexp + ' .+')
    self._test_timeout = re.compile(
        r'Test timeout \([0-9]+ ms\) exceeded for ' + test_name_regexp)
    self._disabled = re.compile(r'\s*YOU HAVE (\d+) DISABLED TEST')
    self._flaky = re.compile(r'\s*YOU HAVE (\d+) FLAKY TEST')

    self._retry_message = re.compile('RETRYING FAILED TESTS:')
    self.retrying_failed = False

    self.TEST_STATUS_MAP = {
      'OK': TEST_SUCCESS_LABEL,
      'failed': TEST_FAILURE_LABEL,
      'timeout': TEST_TIMEOUT_LABEL,
      'warning': TEST_WARNING_LABEL
    }

  def GetCurrentTest(self):
    return self._current_test

  def _StatusOfTest(self, test):
    """Returns the status code for the given test, or 'not known'."""
    test_status = self._test_status.get(test, ('not known', []))
    return test_status[0]

  def _TestsByStatus(self, status, include_fails, include_flaky):
    """Returns list of tests with the given status.

    Args:
      include_fails: If False, tests containing 'FAILS_' anywhere in their
          names will be excluded from the list.
      include_flaky: If False, tests containing 'FLAKY_' anywhere in their
          names will be excluded from the list.
    """
    test_list = [x[0] for x in self._test_status.items()
                 if self._StatusOfTest(x[0]) == status]

    if not include_fails:
      test_list = [x for x in test_list if x.find('FAILS_') == -1]
    if not include_flaky:
      test_list = [x for x in test_list if x.find('FLAKY_') == -1]

    return test_list

  def _RecordError(self, line, reason):
    """Record a log line that produced a parsing error.

    Args:
      line: text of the line at which the error occurred
      reason: a string describing the error
    """
    self._internal_error_lines.append('%s: %s [%s]' %
                                      (self._line_number, line.strip(), reason))

  def RunningTests(self):
    """Returns list of tests that appear to be currently running."""
    return self._TestsByStatus('started', True, True)

  def ParsingErrors(self):
    """Returns a list of lines that have caused parsing errors."""
    return self._internal_error_lines

  def ClearParsingErrors(self):
    """Clears the currently stored parsing errors."""
    self._internal_error_lines = ['Cleared.']

  def PassedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that passed."""
    return self._TestsByStatus('OK', include_fails, include_flaky)

  def FailedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that failed, timed out, or didn't finish
    (crashed).

    This list will be incorrect until the complete log has been processed,
    because it will show currently running tests as having failed.

    Args:
      include_fails: If true, all failing tests with FAILS_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.
      include_flaky: If true, all failing tests with FLAKY_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.

    """
    return (self._TestsByStatus('failed', include_fails, include_flaky) +
            self._TestsByStatus('timeout', True, True) +
            self._TestsByStatus('warning', include_fails, include_flaky) +
            self.RunningTests())

  def TriesForTest(self, test):
    """Returns a list containing the state for all tries of the given test.
    This parser doesn't support retries so a single result is returned."""
    return [self.TEST_STATUS_MAP.get(self._StatusOfTest(test),
                                    TEST_UNKNOWN_LABEL)]

  def DisabledTests(self):
    """Returns the name of the disabled test (if there is only 1) or the number
    of disabled tests.
    """
    return self._disabled_tests

  def FlakyTests(self):
    """Returns the name of the flaky test (if there is only 1) or the number
    of flaky tests.
    """
    return self._flaky_tests

  def FailureDescription(self, test):
    """Returns a list containing the failure description for the given test.

    If the test didn't fail or timeout, returns [].
    """
    test_status = self._test_status.get(test, ('', []))
    return ['%s: ' % test] + test_status[1]

  def CompletedWithoutFailure(self):
    """Returns True if all tests completed and no tests failed unexpectedly."""
    return self.completed and not self.FailedTests()

  def ProcessLine(self, line):
    """This is called once with each line of the test log."""

    # Track line number for error messages.
    self._line_number += 1

    # Some tests (net_unittests in particular) run subprocesses which can write
    # stuff to shared stdout buffer. Sometimes such output appears between new
    # line and gtest directives ('[  RUN  ]', etc) which breaks the parser.
    # Code below tries to detect such cases and recognize a mixed line as two
    # separate lines.

    # List of regexps that parses expects to find at the start of a line but
    # which can be somewhere in the middle.
    gtest_regexps = [
      self._test_start,
      self._test_ok,
      self._test_fail,
      self._test_passed,
    ]

    for regexp in gtest_regexps:
      match = regexp.search(line)
      if match:
        break

    if not match or match.start() == 0:
      self._ProcessLine(line)
    else:
      self._ProcessLine(line[:match.start()])
      self._ProcessLine(line[match.start():])

  def _ProcessLine(self, line):
    """Parses the line and changes the state of parsed tests accordingly.

    Will recognize newly started tests, OK or FAILED statuses, timeouts, etc.
    """

    # Note: When sharding, the number of disabled and flaky tests will be read
    # multiple times, so this will only show the most recent values (but they
    # should all be the same anyway).

    # Is it a line listing the master name?
    if not self.master_name:
      results = self._master_name_re.match(line)
      if results:
        self.master_name = results.group(1)

    results = self._run_test_cases_line.match(line)
    if results:
      # A run_test_cases.py output.
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
      self._current_test = ''
      self._failure_description = []
      return

    # Is it a line declaring all tests passed?
    results = self._test_passed.match(line)
    if results:
      self.completed = True
      self._current_test = ''
      return

    # Is it a line reporting disabled tests?
    results = self._disabled.match(line)
    if results:
      try:
        disabled = int(results.group(1))
      except ValueError:
        disabled = 0
      if disabled > 0 and isinstance(self._disabled_tests, int):
        self._disabled_tests = disabled
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._disabled_tests = 'some'
      return

    # Is it a line reporting flaky tests?
    results = self._flaky.match(line)
    if results:
      try:
        flaky = int(results.group(1))
      except ValueError:
        flaky = 0
      if flaky > 0 and isinstance(self._flaky_tests, int):
        self._flaky_tests = flaky
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._flaky_tests = 'some'
      return

    # Is it the start of a test?
    results = self._test_start.match(line)
    if results:
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
      test_name = results.group(1)
      self._test_status[test_name] = ('started', ['Did not complete.'])
      self._current_test = test_name
      if self.retrying_failed:
        self._failure_description = self._test_status[test_name][1]
        self._failure_description.extend(['', 'RETRY OUTPUT:', ''])
      else:
        self._failure_description = []
      return

    # Is it a test success line?
    results = self._test_ok.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status != 'started':
        self._RecordError(line, 'success while in status %s' % status)
      if self.retrying_failed:
        self._test_status[test_name] = ('warning', self._failure_description)
      else:
        self._test_status[test_name] = ('OK', [])
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test failure line?
    results = self._test_fail.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed', 'timeout'):
        self._RecordError(line, 'failure while in status %s' % status)
      # Don't overwrite the failure description when a failing test is listed a
      # second time in the summary, or if it was already recorded as timing
      # out.
      if status not in ('failed', 'timeout'):
        self._test_status[test_name] = ('failed', self._failure_description)
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test timeout line?
    results = self._test_timeout.search(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed'):
        self._RecordError(line, 'timeout while in status %s' % status)
      self._test_status[test_name] = (
          'timeout', self._failure_description + ['Killed (timed out).'])
      self._failure_description = []
      self._current_test = ''
      return

    # Is it the start of the retry tests?
    results = self._retry_message.match(line)
    if results:
      self.retrying_failed = True
      return

    # Random line: if we're in a test, collect it for the failure description.
    # Tests may run simultaneously, so this might be off, but it's worth a try.
    # This also won't work if a test times out before it begins running.
    if self._current_test:
      self._failure_description.append(line)

    # Parse the "Failing tests:" list at the end of the output, and add any
    # additional failed tests to the list. For example, this includes tests
    # that crash after the OK line.
    if self._parsing_failures:
      results = self._test_name.match(line)
      if results:
        test_name = results.group(1)
        status = self._StatusOfTest(test_name)
        if status in ('not known', 'OK'):
          self._test_status[test_name] = (
              'failed', ['Unknown error, see stdio log.'])
      else:
        self._parsing_failures = False
    elif line.startswith('Failing tests:'):
      self._parsing_failures = True

# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import signal
import time

from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures

_log = logging.getLogger(__name__)


class TestRunException(Exception):

    def __init__(self, code, msg):
        self.code = code
        self.msg = msg


class TestRunResults(object):

    def __init__(self, expectations, num_tests):
        self.total = num_tests
        self.remaining = self.total
        self.expectations = expectations
        self.expected = 0
        self.expected_failures = 0
        self.unexpected = 0
        self.unexpected_failures = 0
        self.unexpected_crashes = 0
        self.unexpected_timeouts = 0
        self.tests_by_expectation = {}
        self.tests_by_timeline = {}
        self.results_by_name = {}  # Map of test name to the last result for the test.
        self.all_results = []  # All results from a run, including every iteration of every test.
        self.unexpected_results_by_name = {}
        self.failures_by_name = {}
        self.total_failures = 0
        self.expected_skips = 0
        for expectation in test_expectations.TestExpectations.EXPECTATIONS.values():
            self.tests_by_expectation[expectation] = set()
        for timeline in test_expectations.TestExpectations.TIMELINES.values():
            self.tests_by_timeline[timeline] = expectations.get_tests_with_timeline(timeline)
        self.slow_tests = set()
        self.interrupted = False
        self.keyboard_interrupted = False
        self.run_time = 0  # The wall clock time spent running the tests (layout_test_runner.run()).

    def add(self, test_result, expected, test_is_slow):
        result_type_for_stats = test_result.type
        if test_expectations.WONTFIX in self.expectations.model().get_expectations(test_result.test_name):
            result_type_for_stats = test_expectations.WONTFIX
        self.tests_by_expectation[result_type_for_stats].add(test_result.test_name)

        self.results_by_name[test_result.test_name] = test_result
        if test_result.type != test_expectations.SKIP:
            self.all_results.append(test_result)
        self.remaining -= 1
        if len(test_result.failures):
            self.total_failures += 1
            self.failures_by_name[test_result.test_name] = test_result.failures
        if expected:
            self.expected += 1
            if test_result.type == test_expectations.SKIP:
                self.expected_skips += 1
            elif test_result.type != test_expectations.PASS:
                self.expected_failures += 1
        else:
            self.unexpected_results_by_name[test_result.test_name] = test_result
            self.unexpected += 1
            if len(test_result.failures):
                self.unexpected_failures += 1
            if test_result.type == test_expectations.CRASH:
                self.unexpected_crashes += 1
            elif test_result.type == test_expectations.TIMEOUT:
                self.unexpected_timeouts += 1
        if test_is_slow:
            self.slow_tests.add(test_result.test_name)


class RunDetails(object):

    def __init__(self, exit_code, summarized_full_results=None,
                 summarized_failing_results=None, initial_results=None,
                 all_retry_results=None, enabled_pixel_tests_in_retry=False):
        self.exit_code = exit_code
        self.summarized_full_results = summarized_full_results
        self.summarized_failing_results = summarized_failing_results
        self.initial_results = initial_results
        self.all_retry_results = all_retry_results or []
        self.enabled_pixel_tests_in_retry = enabled_pixel_tests_in_retry


def _interpret_test_failures(failures):
    test_dict = {}
    failure_types = [type(failure) for failure in failures]
    # FIXME: get rid of all this is_* values once there is a 1:1 map between
    # TestFailure type and test_expectations.EXPECTATION.
    if test_failures.FailureMissingAudio in failure_types:
        test_dict['is_missing_audio'] = True

    if test_failures.FailureMissingResult in failure_types:
        test_dict['is_missing_text'] = True

    if (test_failures.FailureMissingImage in failure_types or
            test_failures.FailureMissingImageHash in failure_types or
            test_failures.FailureReftestNoImageGenerated in failure_types or
            test_failures.FailureReftestNoReferenceImageGenerated in failure_types):
        test_dict['is_missing_image'] = True

    if test_failures.FailureTestHarnessAssertion in failure_types:
        test_dict['is_testharness_test'] = True

    return test_dict


def summarize_results(port_obj, expectations, initial_results,
                      all_retry_results, enabled_pixel_tests_in_retry,
                      only_include_failing=False):
    """Returns a dictionary containing a summary of the test runs, with the following fields:
        'version': a version indicator
        'fixable': The number of fixable tests (NOW - PASS)
        'skipped': The number of skipped tests (NOW & SKIPPED)
        'num_regressions': The number of non-flaky failures
        'num_flaky': The number of flaky failures
        'num_passes': The number of expected and unexpected passes
        'tests': a dict of tests -> {'expected': '...', 'actual': '...'}
    """
    results = {}
    results['version'] = 3
    all_retry_results = all_retry_results or []

    tbe = initial_results.tests_by_expectation
    tbt = initial_results.tests_by_timeline
    results['fixable'] = len(tbt[test_expectations.NOW] - tbe[test_expectations.PASS])
    # FIXME: Remove this. It is redundant with results['num_failures_by_type'].
    results['skipped'] = len(tbt[test_expectations.NOW] & tbe[test_expectations.SKIP])

    num_passes = 0
    num_flaky = 0
    num_regressions = 0
    keywords = {}
    for expectation_string, expectation_enum in test_expectations.TestExpectations.EXPECTATIONS.iteritems():
        keywords[expectation_enum] = expectation_string.upper()

    num_failures_by_type = {}
    for expectation in initial_results.tests_by_expectation:
        tests = initial_results.tests_by_expectation[expectation]
        if expectation != test_expectations.WONTFIX:
            tests &= tbt[test_expectations.NOW]
        num_failures_by_type[keywords[expectation]] = len(tests)
    # The number of failures by type.
    results['num_failures_by_type'] = num_failures_by_type

    tests = {}

    for test_name, result in initial_results.results_by_name.iteritems():
        expected = expectations.get_expectations_string(test_name)
        actual = [keywords[result.type]]
        actual_types = [result.type]
        crash_sites = [result.crash_site]

        if only_include_failing and result.type == test_expectations.SKIP:
            continue

        if result.type == test_expectations.PASS:
            num_passes += 1
            if not result.has_stderr and only_include_failing:
                continue
        elif (result.type != test_expectations.SKIP and
              test_name in initial_results.unexpected_results_by_name):
            # Loop through retry results to collate results and determine
            # whether this is a regression, unexpected pass, or flaky test.
            is_flaky = False
            has_unexpected_pass = False
            for retry_attempt_results in all_retry_results:
                # If a test passes on one of the retries, it won't be in the subsequent retries.
                if test_name not in retry_attempt_results.results_by_name:
                    break

                retry_result = retry_attempt_results.results_by_name[test_name]
                retry_result_type = retry_result.type
                actual.append(keywords[retry_result_type])
                actual_types.append(retry_result_type)
                crash_sites.append(retry_result.crash_site)
                if test_name in retry_attempt_results.unexpected_results_by_name:
                    if retry_result_type == test_expectations.PASS:
                        # The test failed unexpectedly at first, then passed
                        # unexpectedly on a subsequent run -> unexpected pass.
                        has_unexpected_pass = True
                else:
                    # The test failed unexpectedly at first but then ran as
                    # expected on a subsequent run -> flaky.
                    is_flaky = True

            if len(set(actual)) == 1:
                actual = [actual[0]]
                actual_types = [actual_types[0]]

            if is_flaky:
                num_flaky += 1
            elif has_unexpected_pass:
                num_passes += 1
                if not result.has_stderr and only_include_failing:
                    continue
            else:
                # Either no retries or all retries failed unexpectedly.
                num_regressions += 1

        test_dict = {}

        rounded_run_time = round(result.test_run_time, 1)
        if rounded_run_time:
            test_dict['time'] = rounded_run_time

        if result.has_stderr:
            test_dict['has_stderr'] = True

        expectation_line = expectations.model().get_expectation_line(test_name)
        bugs = expectation_line.bugs
        if bugs:
            test_dict['bugs'] = bugs
        if expectation_line.flag_expectations:
            test_dict['flag_expectations'] = expectation_line.flag_expectations

        base_expectations = expectation_line.base_expectations
        if base_expectations:
            test_dict['base_expectations'] = base_expectations

        if result.reftest_type:
            test_dict.update(reftest_type=list(result.reftest_type))

        test_dict['expected'] = expected
        test_dict['actual'] = ' '.join(actual)

        crash_sites = [site for site in crash_sites if site]
        if len(crash_sites) > 0:
            test_dict['crash_site'] = crash_sites[0]

        if test_failures.has_failure_type(test_failures.FailureTextMismatch, result.failures):
            for failure in result.failures:
                if isinstance(failure, test_failures.FailureTextMismatch):
                    test_dict['text_mismatch'] = failure.text_mismatch_category()
                    break

        def is_expected(actual_result):
            return expectations.matches_an_expected_result(test_name, actual_result,
                                                           port_obj.get_option('pixel_tests') or result.reftest_type,
                                                           port_obj.get_option('enable_sanitizer'))

        # To avoid bloating the output results json too much, only add an entry for whether the failure is unexpected.
        if not any(is_expected(actual_result) for actual_result in actual_types):
            test_dict['is_unexpected'] = True

        test_dict.update(_interpret_test_failures(result.failures))

        for retry_attempt_results in all_retry_results:
            retry_result = retry_attempt_results.unexpected_results_by_name.get(test_name)
            if retry_result:
                test_dict.update(_interpret_test_failures(retry_result.failures))

        if result.has_repaint_overlay:
            test_dict['has_repaint_overlay'] = True

        # Store test hierarchically by directory. e.g.
        # foo/bar/baz.html: test_dict
        # foo/bar/baz1.html: test_dict
        #
        # becomes
        # foo: {
        #     bar: {
        #         baz.html: test_dict,
        #         baz1.html: test_dict
        #     }
        # }
        parts = test_name.split('/')
        current_map = tests
        for i, part in enumerate(parts):
            if i == (len(parts) - 1):
                current_map[part] = test_dict
                break
            if part not in current_map:
                current_map[part] = {}
            current_map = current_map[part]

    results['tests'] = tests
    # FIXME: Remove this. It is redundant with results['num_failures_by_type'].
    results['num_passes'] = num_passes
    results['num_flaky'] = num_flaky
    # FIXME: Remove this. It is redundant with results['num_failures_by_type'].
    results['num_regressions'] = num_regressions
    # Does results.html have enough information to compute this itself? (by
    # checking total number of results vs. total number of tests?)
    results['interrupted'] = initial_results.interrupted
    results['layout_tests_dir'] = port_obj.layout_tests_dir()
    results['pixel_tests_enabled'] = port_obj.get_option('pixel_tests')
    results['seconds_since_epoch'] = int(time.time())
    results['build_number'] = port_obj.get_option('build_number')
    results['builder_name'] = port_obj.get_option('builder_name')
    if port_obj.get_option('order') == 'random':
        results['random_order_seed'] = port_obj.get_option('seed')
    results['path_delimiter'] = '/'
    results['flag_name'] = expectations.model().get_flag_name()

    # Don't do this by default since it takes >100ms.
    # It's only used for rebaselining and uploading data to the flakiness dashboard.
    results['chromium_revision'] = ''
    if port_obj.get_option('builder_name'):
        path = port_obj.repository_path()
        git = port_obj.host.git(path=path)
        if git:
            results['chromium_revision'] = str(git.commit_position(path))
        else:
            _log.warning('Failed to determine chromium commit position for %s, '
                         'leaving "chromium_revision" key blank in full_results.json.',
                         path)

    return results

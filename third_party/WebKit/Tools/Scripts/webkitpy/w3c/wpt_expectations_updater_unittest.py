# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import copy

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.buildbot_mock import MockBuildBot
from webkitpy.common.net.layout_test_results import LayoutTestResult, LayoutTestResults
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.log_testing import LoggingTestCase
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater, MARKER_COMMENT


class WPTExpectationsUpdaterTest(LoggingTestCase):

    def setUp(self):
        super(WPTExpectationsUpdaterTest, self).setUp()
        self.host = MockHost()
        self.host.builders = BuilderList({
            'mac': {'port_name': 'test-mac'},
        })

    def test_get_failing_results_dict_only_passing_results(self):
        self.host.buildbot.set_results(Build('mac', 123), LayoutTestResults({
            'tests': {
                'x': {
                    'passing-test.html': {
                        'expected': 'PASS',
                        'actual': 'PASS',
                    },
                },
            },
        }))
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(updater.get_failing_results_dict(Build('mac', 123)), {})

    def test_get_failing_results_dict_no_results(self):
        self.host.buildbot = MockBuildBot()
        self.host.buildbot.set_results(Build('mac', 123), None)
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(updater.get_failing_results_dict(Build('mac', 123)), {})

    def test_get_failing_results_dict_some_failing_results(self):
        self.host.buildbot.set_results(Build('mac', 123), LayoutTestResults({
            'tests': {
                'x': {
                    'failing-test.html': {
                        'expected': 'PASS',
                        'actual': 'IMAGE',
                        'is_unexpected': True,
                    },
                },
            },
        }))
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(updater.get_failing_results_dict(Build('mac', 123)), {
            'x/failing-test.html': {
                'Mac': {
                    'actual': 'IMAGE',
                    'expected': 'PASS',
                    'bug': 'crbug.com/626703',
                },
            },
        })

    def test_merge_same_valued_keys_all_match(self):
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(
            updater.merge_same_valued_keys({
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'PASS'},
            }),
            {('two', 'one'): {'expected': 'FAIL', 'actual': 'PASS'}})

    def test_merge_same_valued_keys_one_mismatch(self):
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(
            updater.merge_same_valued_keys({
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }),
            {
                ('three', 'one'): {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
            })

    def test_get_expectations(self):
        updater = WPTExpectationsUpdater(self.host)
        self.assertEqual(
            updater.get_expectations({'expected': 'FAIL', 'actual': 'PASS'}),
            {'Pass'})
        self.assertEqual(
            updater.get_expectations({'expected': 'FAIL', 'actual': 'TIMEOUT'}),
            {'Timeout'})
        self.assertEqual(
            updater.get_expectations({'expected': 'TIMEOUT', 'actual': 'PASS'}),
            {'Pass'})
        self.assertEqual(
            updater.get_expectations({'expected': 'PASS', 'actual': 'TIMEOUT CRASH FAIL'}),
            {'Crash', 'Failure', 'Timeout'})
        self.assertEqual(
            updater.get_expectations({'expected': 'SLOW CRASH FAIL TIMEOUT', 'actual': 'PASS'}),
            {'Pass'})

    def test_create_line_list_old_tests(self):
        # In this example, there are two failures that are not in w3c tests.
        updater = WPTExpectationsUpdater(self.host)
        results = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'two': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
            }
        }
        self.assertEqual(updater.create_line_list(results), [])

    def test_create_line_list_new_tests(self):
        # In this example, there are unexpected non-fail results in w3c tests.
        updater = WPTExpectationsUpdater(self.host)
        results = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/test'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
            }
        }
        self.assertEqual(
            updater.create_line_list(results),
            [
                'crbug.com/test [ three ] external/fake/test/path.html [ Pass ]',
                'crbug.com/test [ two ] external/fake/test/path.html [ Timeout ]',
                'crbug.com/test [ one ] external/fake/test/path.html [ Pass ]',
            ])

    def test_merge_dicts_with_conflict_raise_exception(self):
        updater = WPTExpectationsUpdater(self.host)
        # Both dicts here have the key "one", and the value is not equal.
        with self.assertRaises(ValueError):
            updater.merge_dicts(
                {
                    'external/fake/test/path.html': {
                        'one': {'expected': 'FAIL', 'actual': 'PASS'},
                        'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                        'three': {'expected': 'FAIL', 'actual': 'PASS'},
                    },
                },
                {
                    'external/fake/test/path.html': {
                        'one': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                    }
                })

    def test_merge_dicts_merges_second_dict_into_first(self):
        updater = WPTExpectationsUpdater(self.host)
        one = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        three = {
            'external/fake/test/path.html': {
                'four': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }

        output = updater.merge_dicts(one, three)
        self.assertEqual(output, one)
        output = updater.merge_dicts(two, three)
        self.assertEqual(output, two)

    def test_generate_results_dict(self):
        updater = WPTExpectationsUpdater(MockHost())
        layout_test_list = [
            LayoutTestResult(
                'test/name.html', {
                    'expected': 'bar',
                    'actual': 'foo',
                    'is_unexpected': True,
                    'has_stderr': True,
                }
            )]
        self.assertEqual(updater.generate_results_dict('dummy_platform', layout_test_list), {
            'test/name.html': {
                'dummy_platform': {
                    'expected': 'bar',
                    'actual': 'foo',
                    'bug': 'crbug.com/626703',
                }
            }
        })

    def test_write_to_test_expectations_with_marker_comment(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = MARKER_COMMENT + '\n'
        updater = WPTExpectationsUpdater(self.host)
        line_list = ['crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]']
        updater.write_to_test_expectations(line_list)
        value = updater.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n'))

    def test_write_to_test_expectations_with_no_marker_comment(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = 'crbug.com/111 [ FakePlatform ]\n'
        updater = WPTExpectationsUpdater(self.host)
        line_list = ['crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]']
        updater.write_to_test_expectations(line_list)
        value = self.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            ('crbug.com/111 [ FakePlatform ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]'))

    def test_write_to_test_expectations_skips_existing_lines(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = 'crbug.com/111 dont/copy/me.html [ Failure ]\n'
        updater = WPTExpectationsUpdater(self.host)
        line_list = [
            'crbug.com/111 dont/copy/me.html [ Failure ]',
            'crbug.com/222 do/copy/me.html [ Failure ]'
        ]
        updater.write_to_test_expectations(line_list)
        value = self.host.filesystem.read_text_file(expectations_path)
        self.assertEqual(
            value,
            ('crbug.com/111 dont/copy/me.html [ Failure ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/222 do/copy/me.html [ Failure ]'))

    def test_write_to_test_expectations_with_marker_and_no_lines(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = (
            MARKER_COMMENT + '\n'
            'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n')
        updater = WPTExpectationsUpdater(self.host)
        updater.write_to_test_expectations([])
        value = updater.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n'))

    def test_is_js_test_true(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(self.host)
        self.assertTrue(updater.is_js_test('foo/bar.html'))

    def test_is_js_test_false(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="ref-test.html"></script>')
        updater = WPTExpectationsUpdater(self.host)
        self.assertFalse(updater.is_js_test('foo/bar.html'))

    def test_is_js_test_non_existent_file(self):
        updater = WPTExpectationsUpdater(self.host)
        self.assertFalse(updater.is_js_test('foo/bar.html'))

    def test_get_test_to_rebaseline_returns_only_tests_with_failures(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/other/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(self.host)
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        tests_to_rebaseline, _ = updater.get_tests_to_rebaseline(two)
        # The other test doesn't have an entry in the test results dict, so it is not listed as a test to rebaseline.
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])

    def test_get_test_to_rebaseline_returns_only_js_tests(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            'this file does not look like a testharness JS test.')
        updater = WPTExpectationsUpdater(self.host)
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/test'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
            }
        }
        tests_to_rebaseline, _ = updater.get_tests_to_rebaseline(two)
        self.assertEqual(tests_to_rebaseline, [])

    def test_get_tests_to_rebaseline_returns_updated_dict(self):
        test_results_dict = {
            'external/fake/test/path.html': {
                'one': {'expected': 'PASS', 'actual': 'TEXT'},
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT'},
            },
        }
        test_results_dict_copy = copy.deepcopy(test_results_dict)
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(self.host)
        tests_to_rebaseline, modified_test_results = updater.get_tests_to_rebaseline(
            test_results_dict)
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(modified_test_results, {
            'external/fake/test/path.html': {
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT'},
            },
        })
        # The original dict isn't modified.
        self.assertEqual(test_results_dict, test_results_dict_copy)

    def test_get_tests_to_rebaseline_also_returns_slow_tests(self):
        test_results_dict = {
            'external/fake/test/path.html': {
                'one': {'expected': 'SLOW', 'actual': 'TEXT'},
                'two': {'expected': 'SLOW', 'actual': 'TIMEOUT'},
            },
        }
        test_results_dict_copy = copy.deepcopy(test_results_dict)
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(self.host)
        tests_to_rebaseline, modified_test_results = updater.get_tests_to_rebaseline(
            test_results_dict)
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(modified_test_results, {
            'external/fake/test/path.html': {
                'two': {'expected': 'SLOW', 'actual': 'TIMEOUT'},
            },
        })
        # The original dict isn't modified.
        self.assertEqual(test_results_dict, test_results_dict_copy)

    def test_run_no_issue_number(self):
        updater = WPTExpectationsUpdater(self.host)
        updater.get_issue_number = lambda: 'None'
        self.assertEqual(1, updater.run(args=[]))
        self.assertLog(['ERROR: No issue on current branch.\n'])

    def test_run_no_try_results(self):
        self.host.web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1],
            }),
            'https://codereview.chromium.org/api/11112222/1': json.dumps({
                'try_job_results': []
            })
        })
        updater = WPTExpectationsUpdater(self.host)
        updater.get_issue_number = lambda: '11112222'
        updater.get_try_bots = lambda: ['test-builder-name']
        self.assertEqual(1, updater.run(args=[]))
        self.assertEqual(
            self.host.web.urls_fetched,
            [
                'https://codereview.chromium.org/api/11112222',
                'https://codereview.chromium.org/api/11112222/1'
            ])
        self.assertLog(['ERROR: No try job information was collected.\n'])

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.net.layouttestresults import LayoutTestResult
from webkitpy.w3c.update_w3c_test_expectations import W3CExpectationsLineAdder


class UpdateW3CTestExpectationsTest(unittest.TestCase, W3CExpectationsLineAdder):

    def setUp(self):
        self.host = MockHost()
        self.mock_dict_one = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_two = {
            'imported/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_three = {
            'imported/fake/test/path.html': {
                'four': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}}
        }
        self.mock_dict_four = {
            'imported/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'}
            }
        }

    def test_merge_same_valued_keys(self):
        self.assertEqual(self.merge_same_valued_keys(self.mock_dict_one['fake/test/path.html']), {
            ('two', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
        })
        self.assertEqual(self.merge_same_valued_keys(self.mock_dict_two['imported/fake/test/path.html']), {
            ('three', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
            'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'}
        })

    def test_get_expectations(self):
        self.assertEqual(self.get_expectations({'expected': 'FAIL', 'actual': 'PASS'}), set(['Pass']))
        self.assertEqual(self.get_expectations({'expected': 'FAIL', 'actual': 'TIMEOUT'}), set(['Timeout']))
        self.assertEqual(self.get_expectations({'expected': 'TIMEOUT', 'actual': 'PASS'}), set(['Pass']))
        self.assertEqual(
            self.get_expectations({'expected': 'PASS', 'actual': 'TIMEOUT CRASH FAIL'}),
            set(['Crash', 'Failure', 'Timeout']))
        self.assertEqual(self.get_expectations({'expected': 'SLOW CRASH FAIL TIMEOUT', 'actual': 'PASS'}), set(['Pass']))

    def test_create_line_list_old_tests(self):
        self.assertEqual(self.create_line_list(self.mock_dict_one), [])

    def test_create_line_list_new_tests(self):
        self.assertEqual(self.create_line_list(self.mock_dict_two),
                         ['crbug.com/626703 [ three ] imported/fake/test/path.html [ Pass ]',
                          'crbug.com/626703 [ two ] imported/fake/test/path.html [ Timeout ]',
                          'crbug.com/626703 [ one ] imported/fake/test/path.html [ Pass ]'])

    def test_merge_dicts_with_conflict_raise_exception(self):
        self.assertRaises(ValueError, self.merge_dicts, self.mock_dict_two, self.mock_dict_four)

    def test_merge_dicts_merges_second_dict_into_first(self):
        output = self.merge_dicts(self.mock_dict_one, self.mock_dict_three)
        self.assertEqual(output, self.mock_dict_one)
        output = self.merge_dicts(self.mock_dict_two, self.mock_dict_three)
        self.assertEqual(output, self.mock_dict_two)

    def test_generate_results_dict(self):
        line_adder = W3CExpectationsLineAdder(MockHost())
        layout_test_list = [LayoutTestResult(
            'test/name.html', {
                'expected': 'bar',
                'actual': 'foo',
                'is_unexpected': True,
                'has_stderr': True
            }
        )]
        platform = 'dummy_platform'
        self.assertEqual(line_adder._generate_results_dict(platform, layout_test_list), {
            'test/name.html': {
                'dummy_platform': {
                    'expected': 'bar',
                    'actual': 'foo',
                    'bug': 'crbug.com/626703'
                }
            }
        })

    def test_write_to_test_expectations_under_comment(self):
        self.host.filesystem.files = {'TestExpectations': '# Tests added from W3C auto import bot\n'}
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = ['fake crbug [ foo ] fake/file/path.html [Pass]']
        path = 'TestExpectations'
        line_adder.write_to_test_expectations(line_adder, path, line_list)
        value = line_adder._host.filesystem.read_text_file(path)
        self.assertEqual(value, '# Tests added from W3C auto import bot\nfake crbug [ foo ] fake/file/path.html [Pass]\n')

    def test_write_to_test_expectations_to_eof(self):
        self.host.filesystem.files['TestExpectations'] = 'not empty\n'
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = ['fake crbug [ foo ] fake/file/path.html [Pass]']
        path = 'TestExpectations'
        line_adder.write_to_test_expectations(line_adder, path, line_list)
        value = line_adder.filesystem.read_text_file(path)
        self.assertEqual(value, 'not empty\n\n# Tests added from W3C auto import bot\nfake crbug [ foo ] fake/file/path.html [Pass]')

    def test_write_to_test_expectations_skip_lines(self):
        self.host.filesystem.files['TestExpectations'] = 'dont copy me\n'
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = ['[ ] dont copy me', '[ ] but copy me']
        path = 'TestExpectations'
        line_adder.write_to_test_expectations(line_adder, path, line_list)
        value = line_adder.filesystem.read_text_file(path)
        self.assertEqual(value, 'dont copy me\n\n# Tests added from W3C auto import bot\n[ ] but copy me')

    def test_is_js_test_true(self):
        self.host.filesystem.files['/mock-checkout/foo/bar.html'] = '''
        <script src="/resources/testharness.js"></script>'''
        finder = WebKitFinder(self.host.filesystem)
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertTrue(line_adder.is_js_test(finder, 'foo/bar.html'))

    def test_is_js_test_false(self):
        self.host.filesystem.files['/mock-checkout/foo/bar.html'] = '''
        <script src="ref-test.html"></script>'''
        finder = WebKitFinder(self.host.filesystem)
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertFalse(line_adder.is_js_test(finder, 'foo/bar.html'))

    def test_get_test_to_rebaseline(self):
        self.host = MockHost()
        self.host.filesystem.files['/mock-checkout/imported/fake/test/path.html'] = '''
                <script src="/resources/testharness.js"></script>'''
        finder = WebKitFinder(self.host.filesystem)
        line_adder = W3CExpectationsLineAdder(self.host)
        tests = ['imported/fake/test/path.html']
        test_dict = {'../../../imported/fake/test/path.html': self.mock_dict_two['imported/fake/test/path.html']}
        tests_to_rebaseline, tests_results = line_adder.get_tests_to_rebaseline(
            finder, tests, test_dict)
        self.assertEqual(tests_to_rebaseline, ['../../../imported/fake/test/path.html'])
        self.assertEqual(tests_results, test_dict)

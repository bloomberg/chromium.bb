# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.layouttestresults import LayoutTestResult
from webkitpy.w3c.update_w3c_test_expectations import W3CExpectationsLineAdder

_log = logging.getLogger(__name__)


class UpdateW3CTestExpectationsTest(unittest.TestCase, W3CExpectationsLineAdder):

    def setUp(self):
        self.mock_dict_one = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_two = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_three = {
            'fake/test/path.html': {
                'four': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}}
        }

    def test_merge_same_valued_keys(self):
        self.assertEqual(self.merge_same_valued_keys(self.mock_dict_one['fake/test/path.html']), {
            ('two', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
        })
        self.assertEqual(self.merge_same_valued_keys(self.mock_dict_two['fake/test/path.html']), {
            ('three', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
            'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'}
        })

    def test_get_expectations(self):
        self.assertEqual(self.get_expectations({'expected': 'FAIL', 'actual': 'PASS'}), ['Pass'])
        self.assertEqual(self.get_expectations({'expected': 'FAIL', 'actual': 'TIMEOUT'}), ['Timeout'])
        self.assertEqual(self.get_expectations({'expected': 'TIMEOUT', 'actual': 'PASS'}), ['Pass', 'Timeout'])

    def test_create_line_list(self):
        self.assertEqual(self.create_line_list(self.mock_dict_one),
                         ['crbug.com/626703 [ two ] fake/test/path.html [ Pass ]',
                          'crbug.com/626703 [ one ] fake/test/path.html [ Pass ]'])
        self.assertEqual(self.create_line_list(self.mock_dict_two),
                         ['crbug.com/626703 [ three ] fake/test/path.html [ Pass ]',
                          'crbug.com/626703 [ two ] fake/test/path.html [ Timeout ]',
                          'crbug.com/626703 [ one ] fake/test/path.html [ Pass ]'])

    def test_merge_dicts_with_conflict_raise_exception(self):
        self.assertRaises(ValueError, self.merge_dicts, self.mock_dict_one, self.mock_dict_two)

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
        host = MockHost()
        line_adder = W3CExpectationsLineAdder(host)
        line_adder._host.filesystem.files = {'TestExpectations': '# Tests added from W3C auto import bot\n'}
        line_list = ['fake crbug [foo] fake/file/path.html [Pass]']
        path = 'TestExpectations'
        line_adder.write_to_test_expectations(line_adder, path, line_list)
        value = line_adder._host.filesystem.read_text_file(path)
        self.assertEqual(value, '# Tests added from W3C auto import bot\nfake crbug [foo] fake/file/path.html [Pass]\n')

    def test_write_to_test_expectations_to_eof(self):
        host = MockHost()
        line_adder = W3CExpectationsLineAdder(host)
        line_adder._host.filesystem.files['TestExpectations'] = 'not empty\n'
        line_list = ['fake crbug [foo] fake/file/path.html [Pass]']
        path = 'TestExpectations'
        line_adder.write_to_test_expectations(line_adder, path, line_list)
        value = line_adder.filesystem.read_text_file(path)
        self.assertEqual(value, 'not empty\n\n# Tests added from W3C auto import bot\nfake crbug [foo] fake/file/path.html [Pass]')

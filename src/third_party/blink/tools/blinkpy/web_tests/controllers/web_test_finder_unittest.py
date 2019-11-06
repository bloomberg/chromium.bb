# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from blinkpy.common import path_finder
from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.controllers import web_test_finder

_MOCK_ROOT = os.path.join(
    path_finder.get_chromium_src_dir(), 'third_party', 'pymock')
sys.path.insert(0, _MOCK_ROOT)
import mock


class WebTestFinderTests(unittest.TestCase):

    def test_find_fastest_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        all_tests = [
            'path/test.html',
            'new/test.html',
            'fast/css/1.html',
            'fast/css/2.html',
            'fast/css/3.html',
            'fast/css/skip1.html',
            'fast/css/skip2.html',
            'fast/css/skip3.html',
            'fast/css/skip4.html',
            'fast/css/skip5.html',
        ]

        port.tests = lambda paths: paths or all_tests

        finder = web_test_finder.WebTestFinder(port, {})
        finder._times_trie = lambda: {
            'fast': {
                'css': {
                    '1.html': 1,
                    '2.html': 2,
                    '3.html': 3,
                    'skip1.html': 0,
                    'skip2.html': 0,
                    'skip3.html': 0,
                    'skip4.html': 0,
                    'skip5.html': 0,
                }
            },
            'path': {
                'test.html': 4,
            }
        }

        tests = finder.find_tests(fastest_percentile=50, args=[])
        self.assertEqual(set(tests[1]), set(['fast/css/1.html', 'fast/css/2.html', 'new/test.html']))

        tests = finder.find_tests(fastest_percentile=50, args=['path/test.html'])
        self.assertEqual(set(tests[1]), set(['fast/css/1.html', 'fast/css/2.html', 'path/test.html', 'new/test.html']))

        tests = finder.find_tests(args=[])
        self.assertEqual(tests[1], all_tests)

        tests = finder.find_tests(args=['path/test.html'])
        self.assertEqual(tests[1], ['path/test.html'])

    def test_find_fastest_tests_excludes_deleted_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        all_tests = [
            'fast/css/1.html',
            'fast/css/2.html',
        ]

        port.tests = lambda paths: paths or all_tests

        finder = web_test_finder.WebTestFinder(port, {})

        finder._times_trie = lambda: {
            'fast': {
                'css': {
                    '1.html': 1,
                    '2.html': 2,
                    'non-existant.html': 1,
                }
            },
        }

        tests = finder.find_tests(fastest_percentile=90, args=[])
        self.assertEqual(set(tests[1]), set(['fast/css/1.html']))

    def test_split_chunks(self):
        split = web_test_finder.WebTestFinder._split_into_chunks  # pylint: disable=protected-access

        with mock.patch('__builtin__.hash', int):

          tests = [1, 2, 3, 4]
          self.assertEqual(([1, 2, 3, 4], []), split(tests, 0, 1))

          self.assertEqual(([2, 4], [1, 3]), split(tests, 0, 2))
          self.assertEqual(([1, 3], [2, 4]), split(tests, 1, 2))

          self.assertEqual(([3], [1, 2, 4]), split(tests, 0, 3))
          self.assertEqual(([1, 4], [2, 3]), split(tests, 1, 3))
          self.assertEqual(([2], [1, 3, 4]), split(tests, 2, 3))

          tests = [1, 2, 3, 4, 5]
          self.assertEqual(([1, 2, 3, 4, 5], []), split(tests, 0, 1))

          self.assertEqual(([2, 4], [1, 3, 5]), split(tests, 0, 2))
          self.assertEqual(([1, 3, 5], [2, 4]), split(tests, 1, 2))

          self.assertEqual(([3], [1, 2, 4, 5]), split(tests, 0, 3))
          self.assertEqual(([1, 4], [2, 3, 5]), split(tests, 1, 3))
          self.assertEqual(([2, 5], [1, 3, 4]), split(tests, 2, 3))

          tests = [1, 2, 3, 4, 5, 6]
          self.assertEqual(([1, 2, 3, 4, 5, 6], []), split(tests, 0, 1))

          self.assertEqual(([2, 4, 6], [1, 3, 5]), split(tests, 0, 2))
          self.assertEqual(([1, 3, 5], [2, 4, 6]), split(tests, 1, 2))

          self.assertEqual(([3, 6], [1, 2, 4, 5]), split(tests, 0, 3))
          self.assertEqual(([1, 4], [2, 3, 5, 6]), split(tests, 1, 3))
          self.assertEqual(([2, 5], [1, 3, 4, 6]), split(tests, 2, 3))

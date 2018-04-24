# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.filesystem_mock import MockFileSystem


class TestPathFinder(unittest.TestCase):

    def test_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder.chromium_base(), '/mock-checkout')

    def test_path_from_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_chromium_base('foo', 'bar.baz'),
            '/mock-checkout/foo/bar.baz')

    def test_layout_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.layout_tests_dir(),
            '/mock-checkout/third_party/WebKit/LayoutTests')

    def test_layout_tests_dir_with_backslash_sep(self):
        filesystem = MockFileSystem()
        filesystem.sep = '\\'
        filesystem.path_to_module = lambda _: (
            'C:\\mock-checkout\\third_party\\blink\\tools\\blinkpy\\foo.py')
        finder = PathFinder(filesystem)
        self.assertEqual(
            finder.layout_tests_dir(),
            'C:\\mock-checkout\\third_party\\WebKit\\LayoutTests')

    def test_perf_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.perf_tests_dir(),
            '/mock-checkout/third_party/blink/perf_tests')

    def test_path_from_layout_tests(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_layout_tests('external', 'wpt'),
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt')

    def test_depot_tools_base_not_found(self):
        filesystem = MockFileSystem()
        filesystem.path_to_module = lambda _: (
            '/mock-checkout/third_party/blink/tools/blinkpy/common/'
            'path_finder.py')
        finder = PathFinder(filesystem)
        self.assertIsNone(finder.depot_tools_base())

    def test_depot_tools_base_exists(self):
        filesystem = MockFileSystem()
        filesystem.path_to_module = lambda _: (
            '/checkout/third_party/blink/tools/blinkpy/common/'
            'path_finder.py')
        filesystem.maybe_make_directory(
            '/checkout/third_party/depot_tools')
        finder = PathFinder(filesystem)
        self.assertEqual(
            finder.depot_tools_base(), '/checkout/third_party/depot_tools')

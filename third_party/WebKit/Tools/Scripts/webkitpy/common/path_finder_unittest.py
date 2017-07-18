# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.path_finder import PathFinder
from webkitpy.common.system.filesystem_mock import MockFileSystem


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
            'C:\\mock-checkout\\third_party\\WebKit\\Tools\\Scripts\\webkitpy\\foo.py')
        finder = PathFinder(filesystem)
        self.assertEqual(
            finder.layout_tests_dir(),
            'C:\\mock-checkout\\third_party\\WebKit\\LayoutTests')

    def test_perf_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.perf_tests_dir(),
            '/mock-checkout/third_party/WebKit/PerformanceTests')

    def test_path_from_layout_tests(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_layout_tests('external', 'wpt'),
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt')

    def test_layout_test_name(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.layout_test_name('third_party/WebKit/LayoutTests/test/name.html'),
            'test/name.html')

    def test_layout_test_name_not_in_layout_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertIsNone(
            finder.layout_test_name('some/other/path/file.html'))

    def test_depot_tools_base_not_found(self):
        finder = PathFinder(MockFileSystem(), sys_path=['/foo'], env_path=['/bar'])
        self.assertIsNone(finder.depot_tools_base())

    def test_depot_tools_base_in_sys_path(self):
        finder = PathFinder(MockFileSystem(), sys_path=['/foo/bar/depot_tools'], env_path=['/bar'])
        self.assertEqual(finder.depot_tools_base(), '/foo/bar/depot_tools')

    def test_depot_tools_base_in_env_path(self):
        finder = PathFinder(MockFileSystem(), sys_path=['/foo'], env_path=['/baz/bin/depot_tools'])
        self.assertEqual(finder.depot_tools_base(), '/baz/bin/depot_tools')

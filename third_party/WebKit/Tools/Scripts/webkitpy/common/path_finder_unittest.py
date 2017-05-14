# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.path_finder import PathFinder
from webkitpy.common.system.filesystem_mock import MockFileSystem


class TestPathFinder(unittest.TestCase):

    # TODO(qyearsley): Add tests for other methods in PathFinder.
    # Including tests for cases when the separator character is backslash.

    def test_layout_test_name(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.layout_test_name('third_party/WebKit/LayoutTests/test/name.html'),
            'test/name.html')

    def test_layout_test_name_not_in_layout_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertIsNone(finder.layout_test_name('some/other/path/file.html'))

    # pylint: disable=protected-access
    def test_webkit_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder._webkit_base(), '/mock-checkout/third_party/WebKit')

    def test_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder.chromium_base(), '/mock-checkout')

    def test_path_from_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_chromium_base('foo', 'bar.baz'),
            '/mock-checkout/foo/bar.baz')

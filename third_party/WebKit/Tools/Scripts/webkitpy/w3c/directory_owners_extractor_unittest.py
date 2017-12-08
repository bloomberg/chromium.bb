# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor


ABS_WPT_BASE = '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'
REL_WPT_BASE = 'third_party/WebKit/LayoutTests/external/wpt'

class DirectoryOwnersExtractorTest(unittest.TestCase):

    def setUp(self):
        self.filesystem = MockFileSystem()
        self.extractor = DirectoryOwnersExtractor(self.filesystem)

    def test_list_owners(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/x.html': '',
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org\nc@chromium.org\n',
            ABS_WPT_BASE + '/bar/x/y.html': '',
            ABS_WPT_BASE + '/bar/OWNERS': 'a@chromium.org\nc@chromium.org\n',
            ABS_WPT_BASE + '/baz/x/y.html': '',
            ABS_WPT_BASE + '/baz/x/OWNERS': 'b@chromium.org\n',
            ABS_WPT_BASE + '/quux/x/y.html': '',
        }
        changed_files = [
            # Same owners:
            REL_WPT_BASE + '/foo/x.html',
            REL_WPT_BASE + '/bar/x/y.html',
            # Same owned directories:
            REL_WPT_BASE + '/baz/x/y.html',
            REL_WPT_BASE + '/baz/x/z.html',
            # Owners not found:
            REL_WPT_BASE + '/quux/x/y.html',
        ]
        self.assertEqual(
            self.extractor.list_owners(changed_files),
            {('a@chromium.org', 'c@chromium.org'): ['external/wpt/bar', 'external/wpt/foo'],
             ('b@chromium.org',): ['external/wpt/baz/x']}
        )

    def test_find_owners_file_current_dir(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org'
        }
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/foo'), ABS_WPT_BASE + '/foo/OWNERS')

    def test_find_owners_file_ancestor(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/x/OWNERS': 'a@chromium.org',
            ABS_WPT_BASE + '/x/y/z.html': '',
        }
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/x/y'), ABS_WPT_BASE + '/x/OWNERS')

    def test_find_owners_file_not_found(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org',
            '/mock-checkout/third_party/WebKit/LayoutTests/external/OWNERS': 'a@chromium.org',
            ABS_WPT_BASE + '/x/y/z.html': '',
        }
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/x/y'), None)

    def test_find_owners_file_skip_empty(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/x/OWNERS': 'a@chromium.org',
            ABS_WPT_BASE + '/x/y/OWNERS': '# b@chromium.org',
            ABS_WPT_BASE + '/x/y/z.html': '',
        }
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/x/y'), ABS_WPT_BASE + '/x/OWNERS')

    def test_find_owners_file_absolute_path(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org'
        }
        self.assertEqual(self.extractor.find_owners_file(ABS_WPT_BASE + '/foo'), ABS_WPT_BASE + '/foo/OWNERS')

    def test_find_owners_file_out_of_tree(self):
        with self.assertRaises(AssertionError):
            self.extractor.find_owners_file('third_party/WebKit/LayoutTests/other')
        self.assertEqual(
            self.extractor.find_owners_file('third_party/WebKit/LayoutTests'), None)
        self.assertEqual(
            self.extractor.find_owners_file('third_party/WebKit/LayoutTests/FlagExpectations/foo-bar'), None)

    def test_extract_owners(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '#This is a comment\n'
            '*\n'
            'foo@chromium.org\n'
            'bar@chromium.org\n'
            'foobar\n'
            '#foobar@chromium.org\n'
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertEqual(self.extractor.extract_owners(ABS_WPT_BASE + '/foo/OWNERS'),
                         ['foo@chromium.org', 'bar@chromium.org'])

    def test_extract_component(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertEqual(self.extractor.extract_component(ABS_WPT_BASE + '/foo/OWNERS'), 'Blink>Layout')

    def test_is_wpt_notify_enabled_true(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# COMPONENT: Blink>Layout\n'
            '# WPT-NOTIFY: true\n'
        }
        self.assertTrue(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

    def test_is_wpt_notify_enabled_false(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# COMPONENT: Blink>Layout\n'
            '# WPT-NOTIFY: false\n'
        }
        self.assertFalse(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

    def test_is_wpt_notify_enabled_absence_is_false(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertFalse(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

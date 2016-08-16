# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.w3c.deps_updater import DepsUpdater
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem


class DepsUpdaterTest(unittest.TestCase):

    def test_parse_try_job_results(self):
        output = """Successes:
linux_builder     http://example.com/linux_builder/builds/222
mac_builder       http://example.com/mac_builder/builds/222
win_builder       http://example.com/win_builder/builds/222
Failures:
android_builder   http://example.com/android_builder/builds/111
chromeos_builder  http://example.com/chromeos_builder/builds/111
Started:
chromeos_generic  http://example.com/chromeos_generic/builds/111
chromeos_daisy    http://example.com/chromeos_daisy/builds/111
Total: 8 tryjobs
"""
        host = MockHost()
        updater = DepsUpdater(host)
        self.assertEqual(updater.parse_try_job_results(output), {
            'Successes': set([
                'mac_builder',
                'win_builder',
                'linux_builder'
            ]),
            'Failures': set([
                'android_builder',
                'chromeos_builder'
            ]),
            'Started': set([
                'chromeos_generic',
                'chromeos_daisy'
            ])
        })

    def test_is_manual_test_regular_test(self):
        updater = DepsUpdater(MockHost())
        fs = MockFileSystem()
        dirname = '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt/a'
        self.assertFalse(updater.is_manual_test(fs, dirname, 'test.html'))
        self.assertFalse(updater.is_manual_test(fs, dirname, 'manual-foo.htm'))
        self.assertFalse(updater.is_manual_test(fs, dirname, 'script.js'))
        self.assertFalse(updater.is_manual_test(fs, dirname, 'foo'))

    def test_is_manual_test_no_automation_file(self):
        updater = DepsUpdater(MockHost())
        fs = MockFileSystem()
        dirname = '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt/a'
        self.assertTrue(updater.is_manual_test(fs, dirname, 'test-manual.html'))
        self.assertTrue(updater.is_manual_test(fs, dirname, 'test-manual.htm'))
        self.assertTrue(updater.is_manual_test(fs, dirname, 'test-manual.xht'))

    def test_is_manual_test_with_corresponding_automation_file(self):
        updater = DepsUpdater(MockHost())
        imported_dir = '/mock-checkout/third_party/WebKit/LayoutTests/imported/'
        fs = MockFileSystem(files={
            imported_dir + 'wpt_automation/a/x-manual-input.js': '',
            imported_dir + 'wpt_automation/a/y-manual-automation.js': '',
        })
        self.assertTrue(updater.is_manual_test(fs, imported_dir + 'wpt/a', 'x-manual.html'))
        self.assertFalse(updater.is_manual_test(fs, imported_dir + 'wpt/a', 'y-manual.html'))

    def test_generate_email_list(self):
        updater = DepsUpdater(MockHost())
        owners = {'foo/bar': 'me@gmail.com', 'foo/baz': 'you@gmail.com', 'foo/bat': 'noone@gmail.com'}
        results = """third_party/WebKit/LayoutTests/foo/bar/file.html
third_party/WebKit/LayoutTests/foo/bar/otherfile.html
third_party/WebKit/LayoutTests/foo/baz/files.html"""
        self.assertEqual(updater.generate_email_list(results, owners), ['me@gmail.com', 'you@gmail.com'])

    def test_parse_directory_owners(self):
        updater = DepsUpdater(MockHost())
        data_file = [
            {'notification-email': 'charizard@gmail.com', 'directory': 'foo/bar'},
            {'notification-email': 'blastoise@gmail.com', 'directory': 'foo/baz'},
            {'notification-email': '', 'directory': 'gol/bat'},
        ]
        self.assertEqual(
            updater.parse_directory_owners(data_file),
            {'foo/bar': 'charizard@gmail.com', 'foo/baz': 'blastoise@gmail.com'})

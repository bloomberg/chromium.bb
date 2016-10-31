# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.w3c.deps_updater import DepsUpdater
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem


class DepsUpdaterTest(unittest.TestCase):

    def test_generate_email_list(self):
        updater = DepsUpdater(MockHost())
        changed_files = [
            'third_party/WebKit/LayoutTests/foo/bar/file.html',
            'third_party/WebKit/LayoutTests/foo/bar/otherfile.html',
            'third_party/WebKit/LayoutTests/foo/baz/files.html',
            'some/non-test.file',
        ]
        directory_to_owner = {
            'foo/bar': 'me@gmail.com',
            'foo/baz': 'you@gmail.com',
            'foo/bat': 'noone@gmail.com',
        }
        self.assertEqual(
            updater.generate_email_list(changed_files, directory_to_owner),
            ['me@gmail.com', 'you@gmail.com'])

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

    def test_update_test_expectations(self):
        host = MockHost()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'] = (
            'Bug(test) some/test/a.html [ Failure ]\n'
            'Bug(test) some/test/b.html [ Failure ]\n'
            'Bug(test) some/test/c.html [ Failure ]\n')
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/VirtualTestSuites'] = '[]'
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/new/a.html'] = ''
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/new/b.html'] = ''
        updater = DepsUpdater(host)
        deleted_tests = ['some/test/b.html']
        renamed_test_pairs = {
            'some/test/a.html': 'new/a.html',
            'some/test/c.html': 'new/c.html',
        }
        updater.update_all_test_expectations_files(deleted_tests, renamed_test_pairs)
        self.assertMultiLineEqual(
            host.filesystem.read_text_file('/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'),
            ('Bug(test) new/a.html [ Failure ]\n'
             'Bug(test) new/c.html [ Failure ]\n'))

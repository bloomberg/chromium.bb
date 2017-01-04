# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.w3c.deps_updater import DepsUpdater


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

    # Tests for protected methods - pylint: disable=protected-access

    def test_cl_description_with_empty_environ(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n')
        updater = DepsUpdater(host)
        description = updater._cl_description()
        self.assertEqual(
            description,
            ('Last commit message\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_with_environ_variables(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n')
        updater = DepsUpdater(host)
        updater.host.environ['BUILDBOT_MASTERNAME'] = 'my.master'
        updater.host.environ['BUILDBOT_BUILDERNAME'] = 'b'
        updater.host.environ['BUILDBOT_BUILDNUMBER'] = '123'
        description = updater._cl_description()
        self.assertEqual(
            description,
            ('Last commit message\n'
             'Build: https://build.chromium.org/p/my.master/builders/b/builds/123\n\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--format=%B']])

    def test_generate_manifest_command_not_found(self):
        # If we're updating csswg-test, then the manifest file won't be found.
        host = MockHost()
        host.filesystem.files = {}
        updater = DepsUpdater(host)
        updater._generate_manifest(
            '/mock-checkout/third_party/WebKit/css',
            '/mock-checkout/third_party/WebKit/LayoutTests/imported/csswg-test')
        self.assertEqual(host.executive.calls, [])

    def test_generate_manifest_successful_run(self):
        # This test doesn't test any aspect of the real manifest script, it just
        # asserts that DepsUpdater._generate_manifest would invoke the script.
        host = MockHost()
        host.filesystem.files = {
            '/mock-checkout/third_party/WebKit/wpt/manifest': 'dummy content'
        }
        updater = DepsUpdater(host)
        updater._generate_manifest(
            '/mock-checkout/third_party/WebKit/wpt',
            '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt')
        self.assertEqual(
            host.executive.calls,
            [
                [
                    '/mock-checkout/third_party/WebKit/wpt/manifest',
                    '--work',
                    '--tests-root',
                    '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt'
                ],
                [
                    'git',
                    'add',
                    '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt/MANIFEST.json'
                ]
            ])

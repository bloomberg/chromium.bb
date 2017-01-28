# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.common import exportable_commits_since


# TODO(qyearsley): Move this to executive_mock.
def mock_command_exec(vals):
    def run_fn(args):
        sub_command = args[1]
        return vals.get(sub_command, '')
    return MockExecutive(run_command_fn=run_fn)


class MockLocalWPT(object):

    def test_patch(self, patch, chromium_commit):  # pylint: disable=unused-argument
        return 'patch'


class CommonTest(unittest.TestCase):

    def test_exportable_commits_since(self):
        host = MockHost()
        host.executive = mock_command_exec({
            'show': 'fake message',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'crrev-parse': 'badbeef8',
            'diff': 'fake diff',
            'diff-tree': 'some\nfiles',
            'format-patch': 'hey I\'m a patch',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(len(commits), 1)
        self.assertIsInstance(commits[0], ChromiumCommit)
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--', 'badbeef8/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'format-patch', '-1', '--stdout', 'badbeef8', '--', 'some', 'files'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8']
        ])

    def test_ignores_commits_with_noexport_true(self):
        host = MockHost()
        host.executive = mock_command_exec({
            'show': 'Commit message\nNOEXPORT=true',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(commits, [])
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8']
        ])

    def test_ignores_reverted_commits_with_noexport_true(self):
        host = MockHost()
        host.executive = mock_command_exec({
            'show': 'Commit message\n> NOEXPORT=true',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(len(commits), 0)
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8']
        ])

    def test_ignores_commits_that_start_with_import(self):
        host = MockHost()
        host.executive = mock_command_exec({
            'show': 'Import rutabaga@deadbeef',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(commits, [])
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8'],
        ])

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


def mock_command_exec(vals):
    def run_fn(args):
        sub_command = args[1]
        return vals.get(sub_command, '')
    return MockExecutive(run_command_fn=run_fn)


class TestExporterTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.wpt_github = MockWPTGitHub(pull_requests=[])

    def test_stops_if_more_than_one_pr_is_in_flight(self):
        host = MockHost()
        wpt_github = MockWPTGitHub(pull_requests=[{'id': 1}, {'id': 2}])

        # TODO: make Exception more specific
        with self.assertRaises(Exception):
            TestExporter(host, wpt_github).run()

    def test_if_pr_exists_merges_it(self):
        host = MockHost()
        wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}])
        TestExporter(host, wpt_github).run()

        self.assertIn('merge_pull_request', wpt_github.calls)

    def test_merge_failure_errors_out(self):
        host = MockHost()
        wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}],
                                   unsuccessful_merge=True)

        # TODO: make Exception more specific
        with self.assertRaises(Exception):
            TestExporter(host, wpt_github).run()

    def test_dry_run_stops_before_creating_pr(self):
        host = MockHost()
        host.executive = MockExecutive(output='beefcafe')
        wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}])
        TestExporter(host, wpt_github, dry_run=True).run()

        self.assertEqual(wpt_github.calls, ['in_flight_pull_requests'])

    def test_creates_pull_request_for_earliest_commit(self):
        host = MockHost()

        def mock_command(args):
            canned_git_outputs = {
                'show': 'newer fake text' if 'cafedad5' in args else 'older fake text',
                'rev-list': 'facebeef\ncafedad5',
                'footers': 'fake-cr-position',
                'remote': 'github',
                'format-patch': 'fake patch',
                'diff': 'fake patch diff',
                'diff-tree': 'fake\n\files\nchanged',
            }
            return canned_git_outputs.get(args[1], '')

        host.executive = MockExecutive(run_command_fn=mock_command)
        wpt_github = MockWPTGitHub(pull_requests=[])

        TestExporter(host, wpt_github).run()

        self.assertEqual(wpt_github.calls, ['in_flight_pull_requests', 'create_pr'])
        self.assertEqual(wpt_github.pull_requests_created,
                         [('chromium-export-try', 'older fake text', 'older fake text')])

    def test_exportable_commits_since(self):
        self.host.executive = mock_command_exec({
            'show': 'fake message',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'crrev-parse': 'badbeef8',
            'diff': 'fake diff',
            'diff-tree': 'some\nfiles',
            'format-patch': 'hey I\'m a patch',
            'footers': 'cr-rev-position',
        })
        test_exporter = TestExporter(self.host, self.wpt_github)

        commits = test_exporter.exportable_commits_since('beefcafe')
        self.assertEqual(len(commits), 1)
        self.assertIsInstance(commits[0], ChromiumCommit)
        self.assertEqual(self.host.executive.calls, [
            ['git', 'clone', 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--', 'badbeef8/third_party/WebKit/LayoutTests/imported/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt'],
            ['git', 'format-patch', '-1', '--stdout', 'badbeef8', '--', 'some', 'files'],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-a'],
            ['git', 'apply', '-'],
            ['git', 'add', '.'],
            ['git', 'diff', 'origin/master'],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-a'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8'],
            ['git', 'show', '--format=%B', '--no-patch', 'badbeef8']])

    def test_ignores_commits_with_noexport_true(self):
        self.host.executive = mock_command_exec({
            'show': 'Commit message\nNOEXPORT=true',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })
        test_exporter = TestExporter(self.host, self.wpt_github)

        commits = test_exporter.exportable_commits_since('beefcafe')
        self.assertEqual(len(commits), 0)
        self.assertEqual(self.host.executive.calls, [
            ['git', 'clone', 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/imported/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt']])

    def test_ignores_reverted_commits_with_noexport_true(self):
        self.host.executive = mock_command_exec({
            'show': 'Commit message\n> NOEXPORT=true',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })
        wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter = TestExporter(self.host, wpt_github)

        commits = test_exporter.exportable_commits_since('beefcafe')
        self.assertEqual(len(commits), 0)
        self.assertEqual(self.host.executive.calls, [
            ['git', 'clone', 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/imported/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt']])

    def test_ignores_commits_that_start_with_import(self):
        self.host.executive = mock_command_exec({
            'show': 'Import rutabaga@deadbeef',
            'rev-list': 'badbeef8',
            'rev-parse': 'badbeef8',
            'footers': 'cr-rev-position',
        })
        wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter = TestExporter(self.host, wpt_github)

        commits = test_exporter.exportable_commits_since('beefcafe')
        self.assertEqual(len(commits), 0)
        self.assertEqual(self.host.executive.calls, [
            ['git', 'clone', 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'badbeef8/third_party/WebKit/LayoutTests/imported/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'badbeef8', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/imported/wpt']])

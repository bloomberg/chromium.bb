# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.wpt_github import PullRequest
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class TestExporterTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()

    def test_merges_more_than_one_pr(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234),
            PullRequest(title='title2', number=5678),
        ])
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, [
            'in_flight_pull_requests',
            'get_pr_branch',
            'merge_pull_request',
            'delete_remote_branch',
            'get_pr_branch',
            'merge_pull_request',
            'delete_remote_branch',
        ])

    def test_merges_all_prs_even_if_one_fails(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234),
            PullRequest(title='title2', number=5678),
        ], unsuccessful_merge_index=0)

        test_exporter.run()
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [5678])
        self.assertEqual(test_exporter.wpt_github.calls, [
            'in_flight_pull_requests',
            'get_pr_branch',
            'merge_pull_request',
            'get_pr_branch',
            'merge_pull_request',
            'delete_remote_branch',
        ])

    def test_dry_run_stops_before_creating_pr(self):
        host = MockHost()
        host.executive = MockExecutive(output='beefcafe')
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', dry_run=True)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234),
        ])
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, ['in_flight_pull_requests'])

    def test_creates_pull_request_for_earliest_commit(self):
        host = MockHost()

        def mock_command(args):
            canned_git_outputs = {
                'show': 'newer fake text' if 'add087a97844f4b9e307d9a216940582d96db306' in args else 'older fake text',
                'rev-list': 'c881563d734a86f7d9cd57ac509653a61c45c240\nadd087a97844f4b9e307d9a216940582d96db306',
                'footers': 'fake-cr-position',
                'remote': 'github',
                'format-patch': 'fake patch',
                'diff': 'fake patch diff',
                'diff-tree': 'fake\n\files\nchanged',
                'crrev-parse': 'c881563d734a86f7d9cd57ac509653a61c45c240',
            }
            return canned_git_outputs.get(args[1], '')

        host.executive = MockExecutive(run_command_fn=mock_command)
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, ['in_flight_pull_requests', 'create_pr'])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created,
                         [('chromium-export-try', 'older fake text', 'older fake text')])

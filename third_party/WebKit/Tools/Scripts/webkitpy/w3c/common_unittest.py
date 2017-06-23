# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import mock_git_commands
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_commit_mock import MockChromiumCommit
from webkitpy.w3c.common import _exportable_commits_since
from webkitpy.w3c.common import is_exportable
from webkitpy.w3c.wpt_github import PullRequest
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class MockLocalWPT(object):

    def test_patch(self, *_):
        return 'patch'


class CommonTest(unittest.TestCase):

    # TODO(qyearsley): Add a test for exportable_commits_over_last_n_commits.

    def test_exportable_commits_since(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'fake message',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306',
            'rev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'crrev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'diff': 'fake diff',
            'diff-tree': 'some\nfiles',
            'format-patch': 'hey I\'m a patch',
            'footers': 'cr-rev-position',
        })

        commits = _exportable_commits_since(
            'beefcafe', host, MockLocalWPT(), MockWPTGitHub(pull_requests=[]))
        self.assertEqual(len(commits), 1)
        self.assertIsInstance(commits[0], ChromiumCommit)
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'add087a97844f4b9e307d9a216940582d96db306/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'footers', '--position', 'add087a97844f4b9e307d9a216940582d96db306'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'add087a97844f4b9e307d9a216940582d96db306', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'format-patch', '-1', '--stdout', 'add087a97844f4b9e307d9a216940582d96db306', '--', 'some', 'files'],
        ])

    def test_is_exportable(self):
        commit = MockChromiumCommit(MockHost(), message='Message')
        github = MockWPTGitHub(pull_requests=[])
        self.assertTrue(is_exportable(commit, MockLocalWPT(), github))

    def test_commit_with_noexport_is_not_exportable(self):
        commit = MockChromiumCommit(MockHost(), message='Message\nNOEXPORT=true')
        github = MockWPTGitHub(pull_requests=[])
        self.assertFalse(is_exportable(commit, MockLocalWPT(), github))

        # A NOEXPORT tag in a revert CL also makes it non-exportable.
        revert = MockChromiumCommit(MockHost(), message='Revert of Message\n> NOEXPORT=true')
        self.assertFalse(is_exportable(revert, MockLocalWPT(), github))

    def test_commit_that_starts_with_import_is_not_exportable(self):
        commit = MockChromiumCommit(MockHost(), message='Import message')
        github = MockWPTGitHub(pull_requests=[])
        self.assertFalse(is_exportable(commit, MockLocalWPT(), github))

    def test_commit_that_has_open_pr_is_exportable(self):
        commit = MockChromiumCommit(
            MockHost(),
            message='Message\nCr-Commit-Position: refs/heads/master@{#423}',
            position='refs/heads/master@{#423}')
        github = MockWPTGitHub(pull_requests=[
            PullRequest('Early PR', 3, 'Commit body\nCr-Commit-Position: refs/heads/master@{#11}', 'closed'),
            PullRequest('Export PR', 12, 'Commit body\nCr-Commit-Position: refs/heads/master@{#423}', 'open'),
        ])
        self.assertTrue(is_exportable(commit, MockLocalWPT(), github))

    def test_commit_that_has_closed_pr_is_not_exportable(self):
        commit = MockChromiumCommit(
            MockHost(),
            message='Message\nCr-Commit-Position: refs/heads/master@{#423}',
            position='refs/heads/master@{#423}')
        github = MockWPTGitHub(pull_requests=[
            PullRequest('Early PR', 3, 'Commit body\nCr-Commit-Position: refs/heads/master@{#11}', 'closed'),
            PullRequest('Export PR', 12, 'Commit body\nCr-Commit-Position: refs/heads/master@{#423}', 'closed'),
        ])
        self.assertFalse(is_exportable(commit, MockLocalWPT(), github))

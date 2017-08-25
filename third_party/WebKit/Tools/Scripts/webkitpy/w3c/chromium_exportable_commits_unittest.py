# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import mock_git_commands
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_commit_mock import MockChromiumCommit
from webkitpy.w3c.chromium_exportable_commits import (
    _exportable_commits_since,
    get_commit_export_state,
    CommitExportState
)
from webkitpy.w3c.wpt_github import PullRequest
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class MockLocalWPT(object):

    def __init__(self, patch_success=True, patch_error=''):
        self.patch_success = patch_success
        self.patch_error = patch_error

    def test_patch(self, _):
        return self.patch_success, self.patch_error


class MultiResponseMockLocalWPT(object):

    def __init__(self, test_results):
        """A mock LocalWPT with canned responses of test_patch.

        Args:
            test_results: a list of (success, error).
        """
        self.test_results = test_results
        self.count = 0

    def test_patch(self, _):
        success, error = self.test_results[self.count]
        self.count += 1
        return success, error


class ChromiumExportableCommitsTest(unittest.TestCase):

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
        }, strict=True)

        commits, _ = _exportable_commits_since(
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

    def test_exportable_commits_since_require_clean_by_default(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'diff-tree': 'some\nfiles',
            'footers': 'cr-rev-position',
            'format-patch': 'hey I\'m a patch',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306\n'
                        'add087a97844f4b9e307d9a216940582d96db307\n'
                        'add087a97844f4b9e307d9a216940582d96db308\n'
        })
        local_wpt = MultiResponseMockLocalWPT([
            (True, ''),
            (False, 'patch failure'),
            (True, ''),
        ])

        commits, _ = _exportable_commits_since(
            'beefcafe', host, local_wpt, MockWPTGitHub(pull_requests=[]))
        self.assertEqual(len(commits), 2)

    def test_exportable_commits_since_without_require_clean(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'diff-tree': 'some\nfiles',
            'footers': 'cr-rev-position',
            'format-patch': 'hey I\'m a patch',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306\n'
                        'add087a97844f4b9e307d9a216940582d96db307\n'
                        'add087a97844f4b9e307d9a216940582d96db308\n'
        })
        local_wpt = MultiResponseMockLocalWPT([
            (True, ''),
            (False, 'patch failure'),
            (True, ''),
        ])

        commits, _ = _exportable_commits_since(
            'beefcafe', host, local_wpt, MockWPTGitHub(pull_requests=[]), require_clean=False)
        self.assertEqual(len(commits), 3)

    def test_get_commit_export_state(self):
        commit = MockChromiumCommit(MockHost())
        github = MockWPTGitHub(pull_requests=[])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(), github), (CommitExportState.EXPORTABLE_CLEAN, ''))

    def test_commit_with_noexport_is_not_exportable(self):
        commit = MockChromiumCommit(MockHost(), body='Message\nNo-Export: true')
        github = MockWPTGitHub(pull_requests=[])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(), github), (CommitExportState.IGNORED, ''))

        # The older NOEXPORT tag also makes it non-exportable.
        old_commit = MockChromiumCommit(MockHost(), body='Message\nNOEXPORT=true')
        self.assertEqual(get_commit_export_state(old_commit, MockLocalWPT(), github), (CommitExportState.IGNORED, ''))

        # No-Export/NOEXPORT in a revert CL also makes it non-exportable.
        revert = MockChromiumCommit(MockHost(), body='Revert of Message\n> No-Export: true')
        self.assertEqual(get_commit_export_state(revert, MockLocalWPT(), github), (CommitExportState.IGNORED, ''))
        old_revert = MockChromiumCommit(MockHost(), body='Revert of Message\n> NOEXPORT=true')
        self.assertEqual(get_commit_export_state(old_revert, MockLocalWPT(), github), (CommitExportState.IGNORED, ''))

    def test_commit_that_starts_with_import_is_not_exportable(self):
        commit = MockChromiumCommit(MockHost(), subject='Import message')
        github = MockWPTGitHub(pull_requests=[])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(), github), (CommitExportState.IGNORED, ''))

    def test_commit_that_has_open_pr_is_exportable(self):
        commit = MockChromiumCommit(MockHost(), change_id='I00decade')
        github = MockWPTGitHub(pull_requests=[
            PullRequest('PR1', 1, 'body\nChange-Id: I00c0ffee', 'closed', []),
            PullRequest('PR2', 2, 'body\nChange-Id: I00decade', 'open', []),
        ])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(), github), (CommitExportState.EXPORTABLE_CLEAN, ''))

    def test_commit_that_has_closed_pr_is_not_exportable(self):
        commit = MockChromiumCommit(MockHost(), change_id='I00decade')
        github = MockWPTGitHub(pull_requests=[
            PullRequest('PR1', 1, 'body\nChange-Id: I00c0ffee', 'closed', []),
            PullRequest('PR2', 2, 'body\nChange-Id: I00decade', 'closed', []),
        ])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(), github), (CommitExportState.EXPORTED, ''))

    def test_commit_that_produces_errors(self):
        commit = MockChromiumCommit(MockHost())
        github = MockWPTGitHub(pull_requests=[])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(False, 'error'), github),
                         (CommitExportState.EXPORTABLE_DIRTY, 'error'))

    def test_commit_that_produces_empty_diff(self):
        commit = MockChromiumCommit(MockHost())
        github = MockWPTGitHub(pull_requests=[])
        self.assertEqual(get_commit_export_state(commit, MockLocalWPT(False, ''), github), (CommitExportState.EXPORTABLE_DIRTY, ''))

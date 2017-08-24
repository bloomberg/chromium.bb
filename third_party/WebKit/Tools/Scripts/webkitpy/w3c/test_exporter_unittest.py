# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive, mock_git_commands
from webkitpy.common.system.log_testing import LoggingTestCase
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_commit_mock import MockChromiumCommit
from webkitpy.w3c.gerrit import GerritCL
from webkitpy.w3c.gerrit_mock import MockGerritAPI
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.wpt_github import PullRequest
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class TestExporterTest(LoggingTestCase):

    def test_dry_run_stops_before_creating_pr(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'crrev-parse': 'c2087acb00eee7960339a0be34ea27d6b20e1131',
        })
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=True)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234, body='', state='open', labels=[]),
        ])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.get_exportable_commits = lambda: ([
            ChromiumCommit(host, position='refs/heads/master@{#458475}'),
            ChromiumCommit(host, position='refs/heads/master@{#458476}'),
            ChromiumCommit(host, position='refs/heads/master@{#458477}'),
        ], [])
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_for_chromium_commit',
            'pr_for_chromium_commit',
            'pr_for_chromium_commit',
        ])

    def test_creates_pull_request_for_all_exportable_commits(self):
        host = MockHost()

        # TODO(robertma): Use MockChromiumCommit instead.
        def mock_command(args):
            canned_git_outputs = {
                'rev-list': '\n'.join([
                    'c881563d734a86f7d9cd57ac509653a61c45c240',
                    'add087a97844f4b9e307d9a216940582d96db306',
                    'cbcd201627f19f81a1697aaaa93cc3a41e644dab',
                ]),
                'footers': 'fake-cr-position',
                'remote': 'github',
                'format-patch': 'fake patch',
                'diff': 'fake patch diff',
                'diff-tree': 'fake\n\files\nchanged',
                'crrev-parse': 'c881563d734a86f7d9cd57ac509653a61c45c240',
            }
            if args[1] == 'show':
                if 'c881563d734a86f7d9cd57ac509653a61c45c240' in args:
                    return 'older fake text'
                elif 'add087a97844f4b9e307d9a216940582d96db306' in args:
                    return 'newer fake text'
                elif 'cbcd201627f19f81a1697aaaa93cc3a41e644dab' in args:
                    return 'newest fake text'
            return canned_git_outputs.get(args[1], '')

        host.executive = MockExecutive(run_command_fn=mock_command)
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None, gerrit_token=None)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[], create_pr_fail_index=1)
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            # get_exportable_commits
            'pr_for_chromium_commit',
            'pr_for_chromium_commit',
            'pr_for_chromium_commit',
            # 1
            'pr_for_chromium_commit',
            'create_pr',
            'add_label "chromium-export"',
            # 2
            'pr_for_chromium_commit',
            'create_pr',
            'add_label "chromium-export"',
            # 3
            'pr_for_chromium_commit',
            'create_pr',
            'add_label "chromium-export"',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [
            ('chromium-export-c881563d73', 'older fake text', 'older fake text'),
            ('chromium-export-cbcd201627', 'newest fake text', 'newest fake text'),
        ])

    def test_creates_and_merges_pull_requests(self):
        # This tests 4 exportable commits:
        # 1. #458475 has a provisional in-flight PR associated with it. The PR needs to be updated but not merged.
        # 2. #458476 has no PR associated with it and should have one created.
        # 3. #458477 has a closed PR associated with it and should be skipped.
        # 4. #458478 has an in-flight PR associated with it and should be merged successfully.
        # 5. #458479 has an in-flight PR associated with it but can not be merged.
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'git show text\nCr-Commit-Position: refs/heads/master@{#458476}\nChange-Id: I0476',
            'crrev-parse': 'c2087acb00eee7960339a0be34ea27d6b20e1131',
        })
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None, gerrit_token=None)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(
                title='Open PR',
                number=1234,
                body='rutabaga\nCr-Commit-Position: refs/heads/master@{#458475}\nChange-Id: I0005',
                state='open',
                labels=['do not merge yet']
            ),
            PullRequest(
                title='Merged PR',
                number=2345,
                body='rutabaga\nCr-Commit-Position: refs/heads/master@{#458477}\nChange-Id: Idead',
                state='closed',
                labels=[]
            ),
            PullRequest(
                title='Open PR',
                number=3456,
                body='rutabaga\nCr-Commit-Position: refs/heads/master@{#458478}\nChange-Id: I0118',
                state='open',
                labels=[]  # It's important that this is empty.
            ),
            PullRequest(
                title='Open PR',
                number=4747,
                body='rutabaga\nCr-Commit-Position: refs/heads/master@{#458479}\nChange-Id: I0147',
                state='open',
                labels=[]  # It's important that this is empty.
            ),
        ], unsuccessful_merge_index=3)  # Mark the last PR as unmergable.
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(host, position='refs/heads/master@{#458475}', change_id='I0005'),
            MockChromiumCommit(host, position='refs/heads/master@{#458476}', change_id='I0476'),
            MockChromiumCommit(host, position='refs/heads/master@{#458477}', change_id='Idead'),
            MockChromiumCommit(host, position='refs/heads/master@{#458478}', change_id='I0118'),
            MockChromiumCommit(host, position='refs/heads/master@{#458479}', change_id='I0147'),
        ], [])
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            # 1. #458475
            'pr_for_chromium_commit',
            'get_pr_branch',
            'update_pr',
            'remove_label "do not merge yet"',
            # 2. #458476
            'pr_for_chromium_commit',
            'create_pr',
            'add_label "chromium-export"',
            # 3. #458477
            'pr_for_chromium_commit',
            # 4. #458478
            'pr_for_chromium_commit',
            # Testing the lack of remove_label here. The exporter should not
            # try to remove the provisional label from PRs it has already
            # removed it from.
            'get_pr_branch',
            'merge_pull_request',
            'delete_remote_branch',
            # 5. #458479
            'pr_for_chromium_commit',
            'get_pr_branch',
            'merge_pull_request',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [
            ('chromium-export-52c3178508', 'Fake subject', 'Fake body\n\nChange-Id: I0476'),
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [3456])

    def test_new_gerrit_cl(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.get = lambda path, raw: base64.b64encode('sample diff')  # pylint: disable=unused-argument
        test_exporter.gerrit.query_exportable_open_cls = lambda: [
            GerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': '1',
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {'commit_with_footers': 'a commit with footers'}
                },
                'owner': {'email': 'test@chromium.org'},
            }, api=test_exporter.gerrit),
        ]
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_with_change_id',
            'create_pr',
            'add_label "chromium-export"',
            'add_label "do not merge yet"',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [
            ('chromium-export-cl-1',
             'subject',
             'a commit with footers\nWPT-Export-Revision: 1'),
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [])

    def test_gerrit_cl_no_update_if_pr_with_same_revision(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: 1',
                        state='open', labels=[]),
        ])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.query_exportable_open_cls = lambda: [
            GerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': '1',
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {'commit_with_footers': 'a commit with footers'}
                },
                'owner': {'email': 'test@chromium.org'},
            }, api=test_exporter.gerrit),
        ]
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_with_change_id',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [])

    def test_gerrit_cl_updates_if_cl_has_new_revision(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: 1',
                        state='open', labels=[]),
        ])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.get = lambda path, raw: base64.b64encode('sample diff')  # pylint: disable=unused-argument
        test_exporter.gerrit.query_exportable_open_cls = lambda: [
            GerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': '1',
                'current_revision': '2',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers 1',
                        'description': 'subject 1',
                    },
                    '2': {
                        'commit_with_footers': 'a commit with footers 2',
                        'description': 'subject 2',
                    },
                },
                'owner': {'email': 'test@chromium.org'},
            }, api=test_exporter.gerrit),
        ]
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_with_change_id',
            'update_pr',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [])

    def test_updates_landed_gerrit_cl(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: 9\nChange-Id: decafbad',
                        state='open', labels=['do not merge yet']),
        ])
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(host, change_id='decafbad'),
        ], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.query_exportable_open_cls = lambda: []
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_for_chromium_commit',
            'get_pr_branch',
            'update_pr',
            'remove_label "do not merge yet"',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [])

    def test_merges_non_provisional_pr(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: 9\nChange-Id: decafbad',
                        state='open', labels=['']),
        ])
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(host, change_id='decafbad'),
        ], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.query_exportable_open_cls = lambda: []
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [
            'pr_for_chromium_commit',
            'get_pr_branch',
            'merge_pull_request',
            'delete_remote_branch',
        ])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [1234])

    def test_does_not_create_pr_if_cl_review_has_not_started(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.get = lambda path, raw: base64.b64encode('sample diff')  # pylint: disable=unused-argument
        test_exporter.gerrit.query_exportable_open_cls = lambda: [
            GerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': '1',
                'current_revision': '2',
                'has_review_started': False,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers 1',
                        'description': 'subject 1',
                    },
                    '2': {
                        'commit_with_footers': 'a commit with footers 2',
                        'description': 'subject 2',
                    },
                },
                'owner': {'email': 'test@chromium.org'},
            }, api=test_exporter.gerrit),
        ]
        success = test_exporter.run()

        self.assertTrue(success)
        self.assertEqual(test_exporter.wpt_github.calls, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created, [])
        self.assertEqual(test_exporter.wpt_github.pull_requests_merged, [])

    def test_run_returns_false_on_patch_failure(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', gerrit_user=None,
                                     gerrit_token=None, dry_run=False)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], ['There was an error with the rutabaga.'])
        test_exporter.gerrit = MockGerritAPI(host, 'gerrit-username', 'gerrit-token')
        test_exporter.gerrit.get = lambda path, raw: base64.b64encode('sample diff')  # pylint: disable=unused-argument
        success = test_exporter.run()

        self.assertFalse(success)
        self.assertLog(['INFO: Cloning GitHub w3c/web-platform-tests into /tmp/wpt\n',
                        'WARNING: There was an error with the rutabaga.\n'])

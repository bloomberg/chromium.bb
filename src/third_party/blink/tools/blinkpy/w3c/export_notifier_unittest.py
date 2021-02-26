# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.gerrit_mock import MockGerritAPI, MockGerritCL
from blinkpy.w3c.export_notifier import ExportNotifier, PRStatusInfo
from blinkpy.w3c.wpt_github import PullRequest
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub


class ExportNotifierTest(LoggingTestCase):
    def setUp(self):
        super(ExportNotifierTest, self).setUp()
        self.host = MockHost()
        self.git = self.host.git()
        self.gerrit = MockGerritAPI()
        self.notifier = ExportNotifier(self.host, self.git, self.gerrit)

    def test_get_gerrit_sha_from_comment_success(self):
        gerrit_comment = self.generate_notifier_comment(
            123, 'checks', 'SHA', None)

        actual = PRStatusInfo.get_gerrit_sha_from_comment(gerrit_comment)

        self.assertEqual(actual, 'SHA')

    def test_get_gerrit_sha_from_comment_fail(self):
        gerrit_comment = 'ABC'

        actual = PRStatusInfo.get_gerrit_sha_from_comment(gerrit_comment)

        self.assertIsNone(actual)

    def test_to_gerrit_comment(self):
        checks_results = {'key1': 'val1', 'key2': 'val2'}
        result_comment = '\nkey2 (val2)\nkey1 (val1)'
        pr_status_info = PRStatusInfo(checks_results, 123, 'SHA')
        expected = self.generate_notifier_comment(123, result_comment, 'SHA',
                                                  None)

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_latest(self):
        checks_results = {'key1': 'val1', 'key2': 'val2'}
        result_comment = '\nkey2 (val2)\nkey1 (val1)'
        pr_status_info = PRStatusInfo(checks_results, 123, None)
        expected = self.generate_notifier_comment(123, result_comment,
                                                  'Latest', None)

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_with_patchset(self):
        checks_results = {'key1': 'val1', 'key2': 'val2'}
        result_comment = '\nkey2 (val2)\nkey1 (val1)'
        pr_status_info = PRStatusInfo(checks_results, 123, 'SHA')
        expected = self.generate_notifier_comment(123, result_comment, 'SHA',
                                                  3)

        actual = pr_status_info.to_gerrit_comment(3)

        self.assertEqual(expected, actual)

    def test_get_relevant_failed_taskcluster_checks_success(self):
        check_runs = [
            {
                "id": "1",
                "conclusion": "failure",
                "name": "wpt-chrome-dev-stability"
            },
            {
                "id": "2",
                "conclusion": "failure",
                "name": "wpt-firefox-nightly-stability"
            },
        ]
        expected = {
            'wpt-chrome-dev-stability':
            'https://github.com/web-platform-tests/wpt/pull/123/checks?check_run_id=1',
            'wpt-firefox-nightly-stability':
            'https://github.com/web-platform-tests/wpt/pull/123/checks?check_run_id=2'
        }

        self.assertEqual(
            self.notifier.get_relevant_failed_taskcluster_checks(
                check_runs, 123), expected)

    def test_get_relevant_failed_taskcluster_checks_empty(self):
        check_runs = [
            {
                "id": "1",
                "conclusion": "success",
                "name": "wpt-chrome-dev-stability"
            },
            {
                "id": "2",
                "conclusion": "failure",
                "name": "infra"
            },
        ]

        self.assertEqual(
            self.notifier.get_relevant_failed_taskcluster_checks(
                check_runs, 123), {})

    def test_has_latest_taskcluster_status_commented_false(self):
        pr_status_info = PRStatusInfo('bar', 123, 'SHA')
        messages = [{
            "date": "2019-08-20 17:42:05.000000000",
            "message": "Uploaded patch set 1.\nInitial upload",
            "_revision_number": 1
        }]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertFalse(actual)

    def test_has_latest_taskcluster_status_commented_true(self):
        pr_status_info = PRStatusInfo({'a': 'b'}, 123, 'SHA')
        messages = [
            {
                "date": "2019-08-20 17:42:05.000000000",
                "message": "Uploaded patch set 1.\nInitial upload",
                "_revision_number": 1
            },
            {
                "date": "2019-08-21 17:41:05.000000000",
                "message":
                self.generate_notifier_comment(123, 'result', 'SHA', 3),
                "_revision_number": 2
            },
        ]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertTrue(actual)

    def test_get_check_runs_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[])
        self.notifier.wpt_github.check_runs = [
            {
                "id": "123",
                "conclusion": "failure",
                "name": "wpt-chrome-dev-stability"
            },
            {
                "id": "456",
                "conclusion": "success",
                "name": "firefox"
            },
        ]
        actual = self.notifier.get_check_runs(123)

        self.assertEqual(len(actual), 2)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'get_pr_branch',
            'get_branch_check_runs',
        ])

    def test_process_failing_prs_success(self):
        checks_results = {'key1': 'val1', 'key2': 'val2'}
        result_comment = '\nkey2 (val2)\nkey1 (val1)'
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'SHA': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        gerrit_dict = {'abc': PRStatusInfo(checks_results, 123, 'SHA')}
        expected = self.generate_notifier_comment(123, result_comment, 'SHA',
                                                  1)

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/abc/revisions/current/review', {
                             'message': expected
                         })])

    def test_process_failing_prs_has_commented(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'bar', 'SHA', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'SHA': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        gerrit_dict = {'abc': PRStatusInfo('bar', 123, 'SHA')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])

    def test_process_failing_prs_with_latest_sha(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'SHA': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        checks_results = {'key1': 'val1', 'key2': 'val2'}
        result_comment = '\nkey2 (val2)\nkey1 (val1)'
        expected = self.generate_notifier_comment(123, result_comment,
                                                  'Latest')
        gerrit_dict = {'abc': PRStatusInfo(checks_results, 123, None)}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/abc/revisions/current/review', {
                             'message': expected
                         })])

    def test_process_failing_prs_raise_gerrit_error(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI(raise_error=True)
        gerrit_dict = {'abc': PRStatusInfo('bar', 'SHA')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])
        self.assertLog([
            'INFO: Processing 1 CLs with failed Taskcluster checks.\n',
            'ERROR: Could not process Gerrit CL abc: Error from query_cl\n'
        ])

    def test_export_notifier_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[])
        self.notifier.wpt_github.recent_failing_pull_requests = [
            PullRequest(
                title='title1',
                number=1234,
                body='description\nWPT-Export-Revision: hash\nChange-Id: decafbad',
                state='open',
                labels=[''])
        ]
        self.notifier.wpt_github.check_runs = [
            {
                "id": "123",
                "conclusion": "failure",
                "name": "wpt-chrome-dev-stability"
            },
            {
                "id": "456",
                "conclusion": "success",
                "name": "firefox"
            },
        ]
        checks_results_comment = ('\nwpt-chrome-dev-stability ('
                                  'https://github.com/web-platform-tests/wpt'
                                  '/pull/1234/checks?check_run_id=123)')

        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'decafbad',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(1234, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'hash': {
                        '_number': 2
                    }
                }
            },
            api=self.notifier.gerrit)
        expected = self.generate_notifier_comment(1234, checks_results_comment,
                                                  'hash', 2)

        exit_code = self.notifier.main()

        self.assertFalse(exit_code)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'recent_failing_chromium_exports',
            'get_pr_branch',
            'get_branch_check_runs',
        ])
        self.assertEqual(self.notifier.gerrit.cls_queried, ['decafbad'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/decafbad/revisions/current/review', {
                             'message': expected
                         })])

    def generate_notifier_comment(self,
                                  pr_number,
                                  checks_results_comment,
                                  sha,
                                  patchset=None):
        if patchset:
            comment = (
                'The exported PR, https://github.com/web-platform-tests/wpt/pull/{}, '
                'has failed the following check(s) on GitHub:\n{}'
                '\n\nThese failures will block the export. '
                'They may represent new or existing problems; please take '
                'a look at the output and see if it can be fixed. '
                'Unresolved failures will be looked at by the Ecosystem-Infra '
                'sheriff after this CL has been landed in Chromium; if you '
                'need earlier help please contact ecosystem-infra@chromium.org.\n\n'
                'Any suggestions to improve this service are welcome; '
                'crbug.com/1027618.\n\n'
                'Gerrit CL SHA: {}\n'
                'Patchset Number: {}').format(pr_number,
                                              checks_results_comment, sha,
                                              patchset)
        else:
            comment = (
                'The exported PR, https://github.com/web-platform-tests/wpt/pull/{}, '
                'has failed the following check(s) on GitHub:\n{}'
                '\n\nThese failures will block the export. '
                'They may represent new or existing problems; please take '
                'a look at the output and see if it can be fixed. '
                'Unresolved failures will be looked at by the Ecosystem-Infra '
                'sheriff after this CL has been landed in Chromium; if you '
                'need earlier help please contact ecosystem-infra@chromium.org.\n\n'
                'Any suggestions to improve this service are welcome; '
                'crbug.com/1027618.\n\n'
                'Gerrit CL SHA: {}').format(pr_number, checks_results_comment,
                                            sha)
        return comment

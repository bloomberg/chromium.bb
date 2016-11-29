# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class TestExporterTest(unittest.TestCase):

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
        host.executive = MockExecutive2(output='beefcafe')
        wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}])
        TestExporter(host, wpt_github, dry_run=True).run()

        self.assertEqual(wpt_github.calls, ['in_flight_pull_requests'])

    def test_creates_pull_request_for_earliest_commit(self):
        host = MockHost()

        def mock_command(args):
            git_command = args[1]
            if git_command == 'rev-list':
                return 'facebeef\ncafedad5'
            elif git_command == 'footers':
                return 'fake-cr-position'
            elif git_command == 'show':
                if 'cafedad5' in args:
                    return 'newer fake text'
                elif 'facebeef' in args:
                    return 'older fake text'
            else:
                return ''

        host.executive = MockExecutive2(run_command_fn=mock_command)
        wpt_github = MockWPTGitHub(pull_requests=[])

        TestExporter(host, wpt_github).run()

        self.assertEqual(wpt_github.calls, ['in_flight_pull_requests', 'create_pr'])
        self.assertEqual(wpt_github.pull_requests_created,
                         [('chromium-export-try', 'older fake text', 'older fake text')])

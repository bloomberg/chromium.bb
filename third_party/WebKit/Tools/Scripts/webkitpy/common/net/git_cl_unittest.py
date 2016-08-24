# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.system.executive_mock import MockExecutive2


class GitCLTest(unittest.TestCase):

    def test_run(self):
        executive = MockExecutive2(output='mock-output')
        git_cl = GitCL(executive)
        output = git_cl.run(['command'])
        self.assertEqual(output, 'mock-output')
        self.assertEqual(executive.calls, [['git', 'cl', 'command']])

    def test_run_with_auth(self):
        executive = MockExecutive2(output='mock-output')
        git_cl = GitCL(executive, auth_refresh_token_json='token.json')
        git_cl.run(['command'])
        self.assertEqual(
            executive.calls,
            [['git', 'cl', 'command', '--auth-refresh-token-json', 'token.json']])

    def test_parse_try_job_results(self):
        output = """Successes:
        linux_builder     http://example.com/linux_builder/builds/222
        mac_builder       http://example.com/mac_builder/builds/222
        win_builder       http://example.com/win_builder/builds/222
        Failures:
        android_builder   http://example.com/android_builder/builds/111
        chromeos_builder  http://example.com/chromeos_builder/builds/111
        Started:
        chromeos_generic  http://example.com/chromeos_generic/builds/111
        chromeos_daisy    http://example.com/chromeos_daisy/builds/111
        Total: 8 tryjobs
        """
        self.assertEqual(GitCL.parse_try_job_results(output), {
            'Successes': set([
                'mac_builder',
                'win_builder',
                'linux_builder'
            ]),
            'Failures': set([
                'android_builder',
                'chromeos_builder'
            ]),
            'Started': set([
                'chromeos_generic',
                'chromeos_daisy'
            ])
        })

    def test_get_issue_number(self):
        git_cl = GitCL(MockExecutive2(output='Issue number: 12345 (http://crrev.com/12345)'))
        self.assertEqual(git_cl.get_issue_number(), '12345')

    def test_get_issue_number_none(self):
        git_cl = GitCL(MockExecutive2(output='Issue number: None (None)'))
        self.assertEqual(git_cl.get_issue_number(), 'None')

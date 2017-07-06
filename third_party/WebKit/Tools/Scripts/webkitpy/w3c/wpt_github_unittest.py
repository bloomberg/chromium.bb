# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.w3c.wpt_github import WPTGitHub, MergeError, GitHubError


class WPTGitHubTest(unittest.TestCase):

    def setUp(self):
        self.wpt_github = WPTGitHub(MockHost(), user='rutabaga', token='decafbad')

    def test_init(self):
        self.assertEqual(self.wpt_github.user, 'rutabaga')
        self.assertEqual(self.wpt_github.token, 'decafbad')

    def test_auth_token(self):
        self.assertEqual(
            self.wpt_github.auth_token(),
            base64.encodestring('rutabaga:decafbad').strip())

    def test_merge_pull_request_throws_merge_error_on_405(self):
        self.wpt_github.host.web.responses = [
            {'status_code': 200},
            {'status_code': 405},
        ]

        self.wpt_github.merge_pull_request(1234)

        with self.assertRaises(MergeError):
            self.wpt_github.merge_pull_request(5678)

    def test_remove_label_throws_github_error_on_non_204(self):
        self.wpt_github.host.web.responses = [
            {'status_code': 200},
        ]

        with self.assertRaises(GitHubError):
            self.wpt_github.remove_label(1234, 'rutabaga')

    def test_delete_remote_branch_throws_github_error_on_non_204(self):
        self.wpt_github.host.web.responses = [
            {'status_code': 200},
        ]

        with self.assertRaises(GitHubError):
            self.wpt_github.delete_remote_branch('rutabaga')

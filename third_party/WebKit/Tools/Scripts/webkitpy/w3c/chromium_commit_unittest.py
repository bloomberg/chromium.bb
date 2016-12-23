# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.test_exporter_unittest import mock_command_exec

CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/imported/wpt/'


class ChromiumCommitTest(unittest.TestCase):

    def test_accepts_sha(self):
        chromium_commit = ChromiumCommit(MockHost(), sha='deadbeefcafe')

        self.assertEqual(chromium_commit.sha, 'deadbeefcafe')
        self.assertIsNone(chromium_commit.position)

    def test_derives_sha_from_position(self):
        host = MockHost()
        host.executive = MockExecutive2(output='deadbeefcafe')
        pos = 'Cr-Commit-Position: refs/heads/master@{#789}'
        chromium_commit = ChromiumCommit(host, position=pos)

        self.assertEqual(chromium_commit.position, 'refs/heads/master@{#789}')
        self.assertEqual(chromium_commit.sha, 'deadbeefcafe')

    def test_filtered_changed_files_blacklist(self):
        host = MockHost()

        fake_files = ['file1', 'MANIFEST.json', 'file3']
        qualified_fake_files = [CHROMIUM_WPT_DIR + f for f in fake_files]

        host.executive = mock_command_exec({
            'diff-tree': '\n'.join(qualified_fake_files),
            'crrev-parse': 'fake rev',
        })

        position_footer = 'Cr-Commit-Position: refs/heads/master@{#789}'
        chromium_commit = ChromiumCommit(host, position=position_footer)

        files = chromium_commit.filtered_changed_files()

        expected_files = ['file1', 'file3']
        qualified_expected_files = [CHROMIUM_WPT_DIR + f for f in expected_files]

        self.assertEqual(files, qualified_expected_files)

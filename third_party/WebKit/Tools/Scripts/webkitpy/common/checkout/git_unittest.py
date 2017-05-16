# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.checkout.git import Git


class GitTestWithMock(unittest.TestCase):

    def make_git(self):
        git = Git(cwd='.', executive=MockExecutive(), filesystem=MockFileSystem())
        git.read_git_config = lambda *args, **kw: 'MOCKKEY:MOCKVALUE'
        return git

    def _assert_timestamp_of_revision(self, canned_git_output, expected):
        git = self.make_git()
        git.find_checkout_root = lambda path: ''
        git.run = lambda args: canned_git_output
        self.assertEqual(git.timestamp_of_revision('some-path', '12345'), expected)

    def test_timestamp_of_revision_utc(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 08:05:49 +0000', '2013-02-08T08:05:49Z')

    def test_timestamp_of_revision_positive_timezone(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 01:02:03 +0130', '2013-02-07T23:32:03Z')

    def test_timestamp_of_revision_pacific_timezone(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 01:55:21 -0800', '2013-02-08T09:55:21Z')

    def test_unstaged_files(self):
        git = self.make_git()
        status_lines = [
            ' M d/modified.txt',
            ' D d/deleted.txt',
            '?? d/untracked.txt',
            '?? a',
            'D  d/deleted.txt',
            'M  d/modified-staged.txt',
            'A  d/added-staged.txt',
        ]
        git.run = lambda args: '\x00'.join(status_lines) + '\x00'
        self.assertEqual(
            git.unstaged_changes(),
            {
                'd/modified.txt': 'M',
                'd/deleted.txt': 'D',
                'd/untracked.txt': '?',
                'a': '?',
            })

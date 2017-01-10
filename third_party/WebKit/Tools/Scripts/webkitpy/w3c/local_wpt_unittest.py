# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.w3c.local_wpt import LocalWPT


class LocalWPTTest(unittest.TestCase):

    def test_constructor_fetches_if_wpt_dir_exists(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/tmp/wpt': ''
        })

        LocalWPT(host)

        self.assertEqual(host.executive.calls, [
            ['git', 'fetch', '--all'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'remote'],
            ['git', 'remote', 'add', 'github', 'git@github.com:w3c/web-platform-tests.git']])

    def test_constructor_clones_if_wpt_dir_does_not_exist(self):
        host = MockHost()
        host.filesystem = MockFileSystem()

        LocalWPT(host)

        self.assertEqual(len(host.executive.calls), 3)
        self.assertEqual(host.executive.calls[0][1], 'clone')

    def test_constructor_no_fetch_flag(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/tmp/wpt': ''
        })

        LocalWPT(host, no_fetch=True)

        self.assertEqual(len(host.executive.calls), 0)

    def test_run(self):
        host = MockHost()
        host.filesystem = MockFileSystem()

        local_wpt = LocalWPT(host)

        local_wpt.run(['echo', 'rutabaga'])
        self.assertEqual(len(host.executive.calls), 4)
        self.assertEqual(host.executive.calls[3], ['echo', 'rutabaga'])

    def test_last_wpt_exported_commit(self):
        host = MockHost()
        return_vals = [
            'deadbeefcafe',
            '123',
            '9ea4fc353a4b1c11c6e524270b11baa4d1ddfde8',
        ]
        host.executive = MockExecutive(run_command_fn=lambda _: return_vals.pop())
        host.filesystem = MockFileSystem()
        local_wpt = LocalWPT(host, no_fetch=True)

        wpt_sha, chromium_commit = local_wpt.most_recent_chromium_commit()
        self.assertEqual(wpt_sha, '9ea4fc353a4b1c11c6e524270b11baa4d1ddfde8')
        self.assertEqual(chromium_commit.position, '123')
        self.assertEqual(chromium_commit.sha, 'deadbeefcafe')

    def test_last_wpt_exported_commit_not_found(self):
        host = MockHost()
        host.executive = MockExecutive(run_command_fn=lambda _: '')
        host.filesystem = MockFileSystem()
        local_wpt = LocalWPT(host)

        commit = local_wpt.most_recent_chromium_commit()
        self.assertEqual(commit, (None, None))

    def test_create_branch_with_patch(self):
        host = MockHost()
        host.filesystem = MockFileSystem()

        local_wpt = LocalWPT(host)

        local_branch_name = local_wpt.create_branch_with_patch('message', 'patch', 'author')
        self.assertEqual(local_branch_name, 'chromium-export-try')
        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'remote'],
            ['git', 'remote', 'add', 'github', 'git@github.com:w3c/web-platform-tests.git'],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-a'],
            ['git', 'branch', '-a'],
            ['git', 'checkout', '-b', 'chromium-export-try'],
            ['git', 'apply', '-'],
            ['git', 'commit', '--author', 'author', '-am', 'message'],
            ['git', 'push', 'github', 'chromium-export-try']])

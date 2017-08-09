# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.executive_mock import MockExecutive, mock_git_commands
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.w3c.local_wpt import LocalWPT


class LocalWPTTest(unittest.TestCase):

    def test_fetch_when_wpt_dir_exists(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/tmp/wpt': ''
        })

        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'fetch', 'origin'],
            ['git', 'checkout', 'origin/master'],
        ])

    def test_fetch_when_wpt_dir_does_not_exist(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/w3c/web-platform-tests.git', '/tmp/wpt'],
        ])

    def test_constructor(self):
        host = MockHost()
        LocalWPT(host, 'token')
        self.assertEqual(len(host.executive.calls), 0)

    def test_run(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.run(['echo', 'rutabaga'])
        self.assertEqual(host.executive.calls, [['echo', 'rutabaga']])

    def test_create_branch_with_patch(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        local_wpt.create_branch_with_patch('chromium-export-decafbad', 'message', 'patch', 'author <author@author.com>')
        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-D', 'chromium-export-decafbad'],
            ['git', 'checkout', '-b', 'chromium-export-decafbad'],
            ['git', 'apply', '-'],
            ['git', 'add', '.'],
            ['git', 'commit', '--author', 'author <author@author.com>', '-am', 'message'],
            ['git', 'push', 'origin', 'chromium-export-decafbad']])

    def test_test_patch_success(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'apply': '',
            'add': '',
            'diff': 'non-trivial patch',
            'reset': '',
            'clean': '',
            'checkout': '',
        }, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (True, ''))

    def test_test_patch_empty_diff(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'apply': '',
            'add': '',
            'diff': '',
            'reset': '',
            'clean': '',
            'checkout': '',
        }, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (False, ''))

    def test_test_patch_error(self):
        def _run_fn(args):
            if args[0] == 'git' and args[1] == 'apply':
                raise ScriptError('MOCK failed applying patch')
            return ''

        host = MockHost()
        host.executive = MockExecutive(run_command_fn=_run_fn)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (False, 'MOCK failed applying patch'))

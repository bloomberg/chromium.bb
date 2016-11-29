# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.chromium_wpt import ChromiumWPT


class ChromiumWPTTest(unittest.TestCase):

    def test_exportable_commits_since(self):
        host = MockHost()

        def mock_command(args):
            git_command = args[1]
            if git_command == 'rev-list':
                return 'badbeef8'
            else:
                return ''

        host.executive = MockExecutive2(run_command_fn=mock_command)

        chromium_wpt = ChromiumWPT(host)
        commits = chromium_wpt.exportable_commits_since('3dadcafe')
        self.assertEqual(len(commits), 1)

    def test_ignores_commits_with_noexport_true(self):
        host = MockHost()

        return_vals = [
            'Commit message\nNOEXPORT=true',  # show (message)
            'deadbeefcafe',  # rev-list
            'third_party/WebKit/LayoutTests/imported/wpt',  # rev-parse
        ]
        host.executive = MockExecutive2(run_command_fn=lambda _: return_vals.pop())

        chromium_wpt = ChromiumWPT(host)
        commits = chromium_wpt.exportable_commits_since('3dadcafe')
        self.assertEqual(len(commits), 0)

    def test_ignores_reverted_commits_with_noexport_true(self):
        host = MockHost()

        return_vals = [
            'Commit message\n> NOEXPORT=true',  # show (message)
            'deadbeefcafe',  # rev-list
            'third_party/WebKit/LayoutTests/imported/wpt',  # rev-parse
        ]
        host.executive = MockExecutive2(run_command_fn=lambda _: return_vals.pop())

        chromium_wpt = ChromiumWPT(host)
        commits = chromium_wpt.exportable_commits_since('3dadcafe')
        self.assertEqual(len(commits), 0)

    def test_ignores_commits_that_start_with_import(self):
        host = MockHost()

        return_vals = [
            'Import rutabaga@deadbeef',  # show (message)
            'deadbeefcafe',  # rev-list
            'third_party/WebKit/LayoutTests/imported/wpt',  # rev-parse
        ]
        host.executive = MockExecutive2(run_command_fn=lambda _: return_vals.pop())

        chromium_wpt = ChromiumWPT(host)
        commits = chromium_wpt.exportable_commits_since('3dadcafe')
        self.assertEqual(len(commits), 0)

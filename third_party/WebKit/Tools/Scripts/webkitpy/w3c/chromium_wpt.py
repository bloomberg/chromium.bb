# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class for interacting with the Chromium git tree
for use cases relating to the Web Platform Tests.
"""

CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/imported/wpt/'

from webkitpy.common.memoized import memoized
from webkitpy.common.webkit_finder import WebKitFinder


class ChromiumWPT(object):

    def __init__(self, host):
        """
        Args:
            host: A Host object.
        """
        self.host = host

    def exportable_commits_since(self, chromium_commit):
        chromium_commits = self.chromium_commits_since(chromium_commit)

        def is_exportable(chromium_commit):
            return self.has_changes_in_wpt(chromium_commit) and not self.is_import_commit(chromium_commit)

        return filter(is_exportable, chromium_commits)

    def is_import_commit(self, chromium_commit):
        return self.subject(chromium_commit).startswith('Import wpt@')

    def has_changes_in_wpt(self, chromium_commit):
        """Detects if a Chromium revision has modified files in the WPT directory."""

        assert chromium_commit
        files = self.host.executive.run_command([
            'git', 'diff-tree', '--no-commit-id',
            '--name-only', '-r', chromium_commit
        ]).splitlines()

        # TODO(jeffcarp): Use DepsUpdater.is_baseline
        return any(f.startswith(CHROMIUM_WPT_DIR) and '-expected' not in f for f in files)

    def chromium_commits_since(self, chromium_commit):
        return self.host.executive.run_command([
            'git', 'rev-list', '--reverse', '{}..HEAD'.format(chromium_commit)
        ]).splitlines()

    def subject(self, chromium_commit):
        return self.host.executive.run_command([
            'git', 'show', '--format=%s', '--no-patch', chromium_commit
        ])

    def commit_position(self, chromium_commit):
        return self.host.executive.run_command([
            'git', 'footers', '--position', chromium_commit
        ])

    def message(self, chromium_commit):
        """Returns a string with a commit's subject and body."""
        return self.host.executive.run_command([
            'git', 'show', '--format=%B', '--no-patch', chromium_commit
        ])

    def format_patch(self, chromium_commit):
        """Makes a patch with just changes in files in the WPT for a given commit."""
        # TODO(jeffcarp): do not include expectations files
        return self.host.executive.run_command([
            'git', 'format-patch', '-1', '--stdout',
            chromium_commit, self.absolute_chromium_wpt_dir()
        ])

    @memoized
    def absolute_chromium_wpt_dir(self):
        finder = WebKitFinder(self.host.filesystem)
        return finder.path_from_webkit_base('LayoutTests', 'imported', 'wpt')

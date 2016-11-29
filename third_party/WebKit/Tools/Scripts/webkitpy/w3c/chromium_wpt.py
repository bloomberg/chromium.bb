# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class for interacting with the Chromium git tree
for use cases relating to the Web Platform Tests.
"""

CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/imported/wpt/'

from webkitpy.common.memoized import memoized
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.deps_updater import DepsUpdater


class ChromiumWPT(object):

    def __init__(self, host):
        """
        Args:
            host: A Host object.
        """
        self.host = host

    def exportable_commits_since(self, commit):
        """Returns SHAs of exportable commits since `commit` in chronological order.

        Args:
            commit: The SHA of the Chromium commit from which this method will look.
        """
        toplevel = self.host.executive.run_command([
            'git', 'rev-parse', '--show-toplevel'
        ]).strip()

        commits = self.host.executive.run_command([
            'git', 'rev-list', '{}..HEAD'.format(commit), '--reverse',
            '--', toplevel + '/' + CHROMIUM_WPT_DIR
        ]).splitlines()

        # TODO(jeffcarp): allow this logic to be shared
        def is_exportable(chromium_commit):
            message = self.message(chromium_commit)
            return (
                'NOEXPORT=true' not in message
                and not message.startswith('Import ')
                # TODO(jeffcarp): change this to allow any commit with
                #   any non-expectation changes to be exportable
                and not self._has_expectations(chromium_commit)
            )

        return filter(is_exportable, commits)

    def _has_expectations(self, chromium_commit):
        files = self.host.executive.run_command([
            'git', 'diff-tree', '--no-commit-id',
            '--name-only', '-r', chromium_commit
        ]).splitlines()

        return any(DepsUpdater.is_baseline(f) for f in files)

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

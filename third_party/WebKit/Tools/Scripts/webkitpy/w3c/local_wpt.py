# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class for interacting with a local checkout of the Web Platform Tests."""

import logging

from webkitpy.common.system.executive import ScriptError
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.common import WPT_GH_REPO_URL_TEMPLATE, CHROMIUM_WPT_DIR


_log = logging.getLogger(__name__)


class LocalWPT(object):

    def __init__(self, host, gh_token=None, path='/tmp/wpt'):
        """
        Args:
            host: A Host object.
            path: Optional, the path to the web-platform-tests repo.
                If this directory already exists, it is assumed that the
                web-platform-tests repo is already checked out at this path.
        """
        self.host = host
        self.path = path
        self.gh_token = gh_token

    def fetch(self):
        assert self.gh_token, 'LocalWPT.gh_token required for fetch'
        if self.host.filesystem.exists(self.path):
            _log.info('WPT checkout exists at %s, fetching latest', self.path)
            self.run(['git', 'fetch', 'origin'])
            self.run(['git', 'checkout', 'origin/master'])
        else:
            _log.info('Cloning GitHub w3c/web-platform-tests into %s', self.path)
            remote_url = WPT_GH_REPO_URL_TEMPLATE.format(self.gh_token)
            self.host.executive.run_command(['git', 'clone', remote_url, self.path])

    def run(self, command, **kwargs):
        """Runs a command in the local WPT directory."""
        return self.host.executive.run_command(command, cwd=self.path, **kwargs)

    def most_recent_chromium_commit(self):
        """Finds the most recent commit in WPT with a Chromium commit position."""
        wpt_commit_hash = self.run(['git', 'rev-list', 'HEAD', '-n', '1', '--grep=Cr-Commit-Position'])
        if not wpt_commit_hash:
            return None, None

        wpt_commit_hash = wpt_commit_hash.strip()
        position = self.run(['git', 'footers', '--position', wpt_commit_hash])
        position = position.strip()
        assert position

        chromium_commit = ChromiumCommit(self.host, position=position)
        return wpt_commit_hash, chromium_commit

    def clean(self):
        self.run(['git', 'reset', '--hard', 'HEAD'])
        self.run(['git', 'clean', '-fdx'])
        self.run(['git', 'checkout', 'origin/master'])

    def create_branch_with_patch(self, branch_name, message, patch, author):
        """Commits the given patch and pushes to the upstream repo.

        Args:
            branch_name: The local and remote git branch name.
            message: Commit message string.
            patch: A patch that can be applied by git apply.
            author: The git commit author.
        """
        self.clean()

        _log.info('Creating local branch %s', branch_name)
        self.run(['git', 'checkout', '-b', branch_name])

        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')

        # TODO(jeffcarp): Use git am -p<n> where n is len(CHROMIUM_WPT_DIR.split(/'))
        # or something not off-by-one.
        self.run(['git', 'apply', '-'], input=patch)
        self.run(['git', 'add', '.'])
        self.run(['git', 'commit', '--author', author, '-am', message])
        self.run(['git', 'push', 'origin', branch_name])

    def test_patch(self, patch, chromium_commit=None):
        """Returns the expected output of a patch against origin/master.

        Args:
            patch: The patch to test against.

        Returns:
            A string containing the diff the patch produced.
        """
        self.clean()

        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')

        try:
            self.run(['git', 'apply', '-'], input=patch)
            self.run(['git', 'add', '.'])
            output = self.run(['git', 'diff', 'origin/master'])
        except ScriptError:
            _log.warning('Patch did not apply cleanly, skipping.')
            if chromium_commit:
                _log.warning('Commit details:\n%s\n%s', chromium_commit.sha,
                             chromium_commit.subject())
            output = ''

        self.clean()
        return output

    def commits_behind_master(self, commit):
        """Returns the number of commits after the given commit on origin/master.

        This doesn't include the given commit, and this assumes that the given
        commit is on the the master branch.
        """
        return len(self.run([
            'git', 'rev-list', '{}..origin/master'.format(commit)
        ]).splitlines())

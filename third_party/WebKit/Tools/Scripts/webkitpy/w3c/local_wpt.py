# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class for interacting with a local checkout of the Web Platform Tests."""

import logging

from webkitpy.common.system.executive import ScriptError
from webkitpy.w3c.common import WPT_GH_SSH_URL_TEMPLATE, WPT_MIRROR_URL, CHROMIUM_WPT_DIR

_log = logging.getLogger(__name__)


class LocalWPT(object):

    def __init__(self, host, gh_token=None, path='/tmp/wpt'):
        """Initializes a LocalWPT instance.

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
        """Fetches a copy of the web-platform-tests repo in `self.path`."""
        if self.host.filesystem.exists(self.path):
            _log.info('WPT checkout exists at %s, fetching latest', self.path)
            self.run(['git', 'fetch', 'origin'])
            self.run(['git', 'checkout', 'origin/master'])
            return

        _log.info('Cloning GitHub w3c/web-platform-tests into %s', self.path)
        if self.gh_token:
            remote_url = WPT_GH_SSH_URL_TEMPLATE.format(self.gh_token)
        else:
            remote_url = WPT_MIRROR_URL
            _log.info('No credentials given, using wpt mirror URL.')
            _log.info('It is possible for the mirror to be delayed; see https://crbug.com/698272.')
        self.host.executive.run_command(['git', 'clone', remote_url, self.path])

    def run(self, command, **kwargs):
        """Runs a command in the local WPT directory."""
        return self.host.executive.run_command(command, cwd=self.path, **kwargs)

    def clean(self):
        """Resets git to a clean state, on origin/master with no changed files."""
        self.run(['git', 'reset', '--hard', 'HEAD'])
        self.run(['git', 'clean', '-fdx'])
        self.run(['git', 'checkout', 'origin/master'])

    def create_branch_with_patch(self, branch_name, message, patch, author, force_push=False):
        """Commits the given patch and pushes to the upstream repo.

        Args:
            branch_name: The local and remote git branch name.
            message: Commit message string.
            patch: A patch that can be applied by git apply.
            author: The git commit author.
            force_push: Applies the -f flag in `git push`.
        """
        self.clean()

        try:
            # This won't be exercised in production because wpt-exporter
            # always runs on a clean machine. But it's useful when running
            # locally since branches stick around.
            _log.info('Deleting old branch %s', branch_name)
            self.run(['git', 'branch', '-D', branch_name])
        except ScriptError:
            # This might mean the branch wasn't found. Ignore this error.
            pass

        _log.info('Creating local branch %s', branch_name)
        self.run(['git', 'checkout', '-b', branch_name])

        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')

        _log.info('Author: %s', author)
        if '<' in author:
            author_str = author
        else:
            author_str = '%s <%s>' % (author, author)

        # TODO(jeffcarp): Use git am -p<n> where n is len(CHROMIUM_WPT_DIR.split(/'))
        # or something not off-by-one.
        self.run(['git', 'apply', '-'], input=patch)
        self.run(['git', 'add', '.'])
        self.run(['git', 'commit', '--author', author_str, '-am', message])

        # Force push is necessary when updating a PR with a new patch
        # from Gerrit.
        if force_push:
            self.run(['git', 'push', '-f', 'origin', branch_name])
        else:
            self.run(['git', 'push', 'origin', branch_name])

    def test_patch(self, patch):
        """Tests whether a patch can be cleanly applied against origin/master.

        Args:
            patch: The patch to test against.

        Returns:
            (success, error): success is True if and only if the patch can be
            cleanly applied and produce non-empty diff; error is a string of
            error messages when the patch fails to apply, empty otherwise.
        """
        self.clean()
        error = self.apply_patch(patch)
        diff = self.run(['git', 'diff', 'origin/master'])
        self.clean()
        if error != '':
            return False, error
        if diff == '':
            # No error message is returned for empty diff. The patch might be
            # empty or has been exported.
            return False, ''
        return True, ''

    def apply_patch(self, patch):
        """Applies a Chromium patch to the local WPT repo and stages.

        Returns:
            A string containing error messages from git, empty if the patch applies cleanly.
        """
        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')
        try:
            self.run(['git', 'apply', '-'], input=patch)
            self.run(['git', 'add', '.'])
        except ScriptError as error:
            return error.message
        return ''

    def commits_behind_master(self, commit):
        """Returns the number of commits after the given commit on origin/master.

        This doesn't include the given commit, and this assumes that the given
        commit is on the the master branch.
        """
        return len(self.run([
            'git', 'rev-list', '{}..origin/master'.format(commit)
        ]).splitlines())

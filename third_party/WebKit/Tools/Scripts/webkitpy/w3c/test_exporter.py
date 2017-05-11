# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.common import exportable_commits_over_last_n_commits
from webkitpy.w3c.gerrit import Gerrit
from webkitpy.w3c.wpt_github import WPTGitHub, MergeError

_log = logging.getLogger(__name__)

PR_HISTORY_WINDOW = 100
COMMIT_HISTORY_WINDOW = 5000
WPT_URL = 'https://github.com/w3c/web-platform-tests/'


class TestExporter(object):

    def __init__(self, host, gh_user, gh_token, gerrit_user, gerrit_token, dry_run=False):
        self.host = host
        self.wpt_github = WPTGitHub(host, gh_user, gh_token, pr_history_window=PR_HISTORY_WINDOW)

        self.gerrit = Gerrit(self.host, gerrit_user, gerrit_token)

        self.dry_run = dry_run
        self.local_wpt = LocalWPT(self.host, gh_token)
        self.local_wpt.fetch()

    def run(self):
        """For last n commits on Chromium master, create or try to merge a PR.

        The exporter will look in chronological order at every commit in Chromium.
        """
        open_gerrit_cls = self.gerrit.query_open_cls()
        self.process_gerrit_cls(open_gerrit_cls)

        exportable_commits = self.get_exportable_commits(limit=COMMIT_HISTORY_WINDOW)
        for exportable_commit in exportable_commits:
            pull_request = self.wpt_github.pr_with_position(exportable_commit.position)
            if pull_request:
                if pull_request.state == 'open':
                    self.merge_pull_request(pull_request)
                    # TODO(jeffcarp): if this was from Gerrit, comment back on the Gerrit CL that the PR was merged
                else:
                    _log.info('Pull request is not open: #%d %s', pull_request.number, pull_request.title)
            else:
                self.create_pull_request(exportable_commit)

    def process_gerrit_cls(self, gerrit_cls):
        """Iterates through Gerrit CLs and prints their statuses.

        Right now this method does nothing. In the future it will create PRs for CLs and help
        transition them when they're landed.
        """
        for cl in gerrit_cls:
            cl_url = 'https://chromium-review.googlesource.com/c/%s' % cl['_number']
            _log.info('Found Gerrit in-flight CL: "%s" %s', cl['subject'], cl_url)

            # Check if CL already has a corresponding PR
            pull_request = self.wpt_github.pr_with_change_id(cl['change_id'])

            if pull_request:
                pr_url = '{}pull/{}'.format(WPT_URL, pull_request.number)
                _log.info('In-flight PR found: %s', pr_url)
            else:
                _log.info('No in-flight PR found for CL.')


    def get_exportable_commits(self, limit):
        return exportable_commits_over_last_n_commits(limit, self.host, self.local_wpt)

    def merge_pull_request(self, pull_request):
        _log.info('In-flight PR found: %s', pull_request.title)
        _log.info('https://github.com/w3c/web-platform-tests/pull/%d', pull_request.number)

        if self.dry_run:
            _log.info('[dry_run] Would have attempted to merge PR')
            return

        _log.info('Attempting to merge...')

        # This is outside of the try block because if there's a problem communicating
        # with the GitHub API, we should hard fail.
        branch = self.wpt_github.get_pr_branch(pull_request.number)

        try:
            self.wpt_github.merge_pull_request(pull_request.number)

            # This is in the try block because if a PR can't be merged, we shouldn't
            # delete its branch.
            self.wpt_github.delete_remote_branch(branch)
        except MergeError:
            _log.info('Could not merge PR.')

    def export_first_exportable_commit(self):
        """Looks for exportable commits in Chromium, creates PR if found."""

        wpt_commit, chromium_commit = self.local_wpt.most_recent_chromium_commit()
        assert chromium_commit, 'No Chromium commit found, this is impossible'

        wpt_behind_master = self.local_wpt.commits_behind_master(wpt_commit)

        _log.info('\nLast Chromium export commit in web-platform-tests:')
        _log.info('web-platform-tests@%s', wpt_commit)
        _log.info('(%d behind web-platform-tests@origin/master)', wpt_behind_master)

        _log.info('\nThe above WPT commit points to the following Chromium commit:')
        _log.info('chromium@%s', chromium_commit.sha)
        _log.info('(%d behind chromium@origin/master)', chromium_commit.num_behind_master())

        exportable_commits = exportable_commits_since(chromium_commit.sha, self.host, self.local_wpt)

        if not exportable_commits:
            _log.info('No exportable commits found in Chromium, stopping.')
            return

        _log.info('Found %d exportable commits in Chromium:', len(exportable_commits))
        for commit in exportable_commits:
            _log.info('- %s %s', commit, commit.subject())

        outbound_commit = exportable_commits[0]
        _log.info('Picking the earliest commit and creating a PR')
        _log.info('- %s %s', outbound_commit.sha, outbound_commit.subject())

        self.create_pull_request(outbound_commit)

    def create_pull_request(self, outbound_commit):
        patch = outbound_commit.format_patch()
        message = outbound_commit.message()
        author = outbound_commit.author()

        if self.dry_run:
            _log.info('[dry_run] Stopping before creating PR')
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.info('\n\n[dry_run] patch:')
            _log.info(patch)
            return

        branch_name = 'chromium-export-{sha}'.format(sha=outbound_commit.short_sha)
        self.local_wpt.create_branch_with_patch(branch_name, message, patch, author)

        response_data = self.wpt_github.create_pr(
            remote_branch_name=branch_name,
            desc_title=outbound_commit.subject(),
            body=outbound_commit.body())

        _log.info('Create PR response: %s', response_data)

        if response_data:
            data, status_code = self.wpt_github.add_label(response_data['number'])
            _log.info('Add label response (status %s): %s', status_code, data)

        return response_data

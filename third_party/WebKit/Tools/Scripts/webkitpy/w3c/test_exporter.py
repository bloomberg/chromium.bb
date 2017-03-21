# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.common import exportable_commits_since
from webkitpy.w3c.wpt_github import WPTGitHub, MergeError

_log = logging.getLogger(__name__)


class TestExporter(object):

    def __init__(self, host, gh_user, gh_token, dry_run=False):
        self.host = host
        self.wpt_github = WPTGitHub(host, gh_user, gh_token)
        self.dry_run = dry_run
        self.local_wpt = LocalWPT(self.host, gh_token)
        self.local_wpt.fetch()

    def run(self):
        """Query in-flight pull requests, then merges PR or creates one.

        This script assumes it will be run on a regular interval. On
        each invocation, it will either attempt to merge or attempt to
        create a PR, never both.
        """
        pull_requests = self.wpt_github.in_flight_pull_requests()
        self.merge_all_pull_requests(pull_requests)

        # TODO(jeffcarp): The below line will enforce draining all open PRs before
        # adding any more to the queue, which mirrors current behavior. After this
        # change lands, modify the following to:
        # - for each exportable commit
        #   - check if there's a corresponding PR
        #   - if not, create one
        if not pull_requests:
            _log.info('No in-flight PRs found, looking for exportable commits.')
            self.export_first_exportable_commit()

    def merge_all_pull_requests(self, pull_requests):
        for pr in pull_requests:
            self.merge_pull_request(pr)

    def merge_pull_request(self, pull_request):
        _log.info('In-flight PR found: %s', pull_request.title)
        _log.info('https://github.com/w3c/web-platform-tests/pull/%d', pull_request.number)
        _log.info('Attempting to merge...')

        if self.dry_run:
            _log.info('[dry_run] Would have attempted to merge PR')
            return

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

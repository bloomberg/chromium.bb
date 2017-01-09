# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.chromium_commit import ChromiumCommit

_log = logging.getLogger(__name__)

CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/imported/wpt/'

# TODO(jeffcarp): have the script running this fetch Chromium origin/master
# TODO(jeffcarp): move WPT fetch out of its constructor to match planned ChromiumWPT pattern

class TestExporter(object):

    def __init__(self, host, wpt_github, dry_run=False):
        self.host = host
        self.wpt_github = wpt_github
        self.dry_run = dry_run
        self.local_wpt = LocalWPT(self.host)

    def run(self):
        """Query in-flight pull requests, then merge PR or create one.

        This script assumes it will be run on a regular interval. On
        each invocation, it will either attempt to merge or attempt to
        create a PR, never both.
        """
        pull_requests = self.wpt_github.in_flight_pull_requests()

        if len(pull_requests) == 1:
            self.merge_in_flight_pull_request(pull_requests.pop())
        elif len(pull_requests) > 1:
            _log.error(pull_requests)
            # TODO(jeffcarp): Print links to PRs
            raise Exception('More than two in-flight PRs!')
        else:
            self.export_first_exportable_commit()

    def merge_in_flight_pull_request(self, pull_request):
        """Attempt to merge an in-flight PR.

        Args:
            pull_request: a PR object returned from the GitHub API.
        """

        _log.info('In-flight PR found: #%d', pull_request['number'])
        _log.info(pull_request['title'])

        # TODO(jeffcarp): Check the PR status here (for Travis CI, etc.)

        if self.dry_run:
            _log.info('[dry_run] Would have attempted to merge PR')
            return

        _log.info('Merging...')
        self.wpt_github.merge_pull_request(pull_request['number'])
        _log.info('PR merged! Deleting branch.')
        self.wpt_github.delete_remote_branch('chromium-export-try')
        _log.info('Branch deleted!')

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

        exportable_commits = self.exportable_commits_since(chromium_commit.sha)

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

        if self.dry_run:
            _log.info('[dry_run] Stopping before creating PR')
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.info('\n\n[dry_run] patch:')
            _log.info(patch)
            return

        remote_branch_name = self.local_wpt.create_branch_with_patch(message, patch)

        response_data = self.wpt_github.create_pr(
            remote_branch_name=remote_branch_name,
            desc_title=outbound_commit.subject(),
            body=outbound_commit.body())

        _log.info('Create PR response: %s', response_data)

        if response_data:
            data, status_code = self.wpt_github.add_label(response_data['number'])
            _log.info('Add label response (status %s): %s', status_code, data)

    def exportable_commits_since(self, commit):
        """Returns SHAs of exportable commits since `commit` in chronological order.

        Args:
            commit: The SHA of the Chromium commit from which this method will look.
        """
        repo_root = self.host.executive.run_command([
            'git', 'rev-parse', '--show-toplevel'
        ]).strip()

        commits = self.host.executive.run_command([
            'git', 'rev-list', '{}..HEAD'.format(commit), '--reverse',
            '--', repo_root + '/' + CHROMIUM_WPT_DIR
        ]).splitlines()

        chromium_commits = [ChromiumCommit(self.host, sha=c) for c in commits]

        def is_exportable(chromium_commit):
            patch = chromium_commit.format_patch()
            return (
                patch
                and self.local_wpt.test_patch(patch)
                and 'NOEXPORT=true' not in chromium_commit.message()
                and not chromium_commit.message().startswith('Import ')
            )

        return [c for c in chromium_commits if is_exportable(c)]

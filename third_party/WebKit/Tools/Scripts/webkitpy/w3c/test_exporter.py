# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.chromium_wpt import ChromiumWPT
from webkitpy.w3c.chromium_commit import ChromiumCommit

_log = logging.getLogger(__name__)


class TestExporter(object):

    def __init__(self, host, wpt_github, dry_run=False):
        self.host = host
        self.wpt_github = wpt_github
        self.dry_run = dry_run

    def run(self):
        # First, poll for an in-flight pull request and merge if exists
        pull_requests = self.wpt_github.in_flight_pull_requests()

        if len(pull_requests) == 1:
            pull_request = pull_requests.pop()

            _log.info('In-flight PR found: #%d', pull_request['number'])
            _log.info(pull_request['title'])

            # TODO(jeffcarp): Check the PR status here

            if self.dry_run:
                _log.info('[dry_run] Would have attempted to merge PR')
            else:
                _log.info('Merging...')
                self.wpt_github.merge_pull_request(pull_request['number'])
                _log.info('PR merged!')
        elif len(pull_requests) > 1:
            _log.error(pull_requests)
            # TODO(jeffcarp): Print links to PRs
            raise Exception('More than two in-flight PRs!')

        # Second, look for exportable commits in Chromium
        # At this point, no in-flight PRs should exist
        # If there was an issue merging, it should have errored out
        local_wpt = LocalWPT(self.host, use_github=False)
        chromium_wpt = ChromiumWPT(self.host)

        # TODO(jeffcarp): have the script running this fetch Chromium origin/master
        # TODO(jeffcarp): move WPT fetch out of its constructor to match planned ChromiumWPT pattern

        wpt_commit, chromium_commit = local_wpt.most_recent_chromium_commit()
        assert chromium_commit, 'No Chromium commit found, this is impossible'

        wpt_behind_master = local_wpt.commits_behind_master(wpt_commit)

        _log.info('\nLast Chromium export commit in web-platform-tests:')
        _log.info('web-platform-tests@%s', wpt_commit)
        _log.info('(%d behind web-platform-tests@origin/master)', wpt_behind_master)

        _log.info('\nThe above WPT commit points to the following Chromium commit:')
        _log.info('chromium@%s', chromium_commit.sha)
        _log.info('(%d behind chromium@origin/master)', chromium_commit.num_behind_master())

        # TODO(jeffcarp): Have this function return ChromiumCommits
        exportable_commits = chromium_wpt.exportable_commits_since(chromium_commit.sha)

        if not exportable_commits:
            _log.info('No exportable commits found in Chromium, stopping.')
            return

        _log.info('Found %d exportable commits in Chromium:', len(exportable_commits))
        for commit in exportable_commits:
            _log.info('- %s %s', commit, chromium_wpt.subject(commit))

        outbound_commit = ChromiumCommit(self.host, sha=exportable_commits[0])
        _log.info('Picking the earliest commit and creating a PR')
        _log.info('- %s %s', outbound_commit.sha, outbound_commit.subject())

        patch = outbound_commit.format_patch()
        message = outbound_commit.message()

        # TODO: now do a test comparison of patch against local WPT

        if self.dry_run:
            _log.info('[dry_run] Stopping before creating PR')
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.info('\n\n[dry_run] patch:')
            _log.info(patch)
            return

        local_branch_name = local_wpt.create_branch_with_patch(message, patch)

        self.wpt_github.create_pr(
            local_branch_name=local_branch_name,
            desc_title=outbound_commit.subject(),
            body=outbound_commit.body())

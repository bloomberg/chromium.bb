# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.common import (
    exportable_commits_over_last_n_commits,
    WPT_GH_URL,
    WPT_REVISION_FOOTER
)
from webkitpy.w3c.gerrit import GerritAPI, GerritCL
from webkitpy.w3c.wpt_github import WPTGitHub, MergeError

_log = logging.getLogger(__name__)

PR_HISTORY_WINDOW = 100
COMMIT_HISTORY_WINDOW = 5000


class TestExporter(object):

    def __init__(self, host, gh_user, gh_token, gerrit_user, gerrit_token, dry_run=False):
        self.host = host
        self.wpt_github = WPTGitHub(host, gh_user, gh_token, pr_history_window=PR_HISTORY_WINDOW)

        self.gerrit = GerritAPI(self.host, gerrit_user, gerrit_token)

        self.dry_run = dry_run
        self.local_wpt = LocalWPT(self.host, gh_token)
        self.local_wpt.fetch()

    def run(self):
        """For last n commits on Chromium master, create or try to merge a PR.

        The exporter will look in chronological order at every commit in Chromium.
        """
        open_gerrit_cls = self.gerrit.query_exportable_open_cls()
        self.process_gerrit_cls(open_gerrit_cls)

        exportable_commits = self.get_exportable_commits(limit=COMMIT_HISTORY_WINDOW)
        for exportable_commit in exportable_commits:
            pull_request = self.corresponding_pull_request_for_commit(exportable_commit)

            if pull_request:
                if pull_request.state == 'open':
                    self.merge_pull_request(pull_request)
                else:
                    _log.info('Pull request is not open: #%d %s', pull_request.number, pull_request.title)
            else:
                self.create_pull_request(exportable_commit)

    def process_gerrit_cls(self, gerrit_cls):
        """Creates or updates PRs for Gerrit CLs."""
        for cl in gerrit_cls:
            _log.info('Found Gerrit in-flight CL: "%s" %s', cl.subject, cl.url)

            # Check if CL already has a corresponding PR
            pull_request = self.wpt_github.pr_with_change_id(cl.change_id)

            if pull_request:
                pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
                _log.info('In-flight PR found: %s', pr_url)

                pr_cl_revision = self.wpt_github.extract_metadata(WPT_REVISION_FOOTER + ' ', pull_request.body)
                if cl.current_revision_sha == pr_cl_revision:
                    _log.info('PR revision matches CL revision. Nothing to do here.')
                    continue

                _log.info('New revision found, updating PR...')
                self.create_or_update_pull_request_from_cl(cl, pull_request)
            else:
                _log.info('No in-flight PR found for CL. Creating...')
                self.create_or_update_pull_request_from_cl(cl)

    def get_exportable_commits(self, limit):
        return exportable_commits_over_last_n_commits(limit, self.host, self.local_wpt)

    def merge_pull_request(self, pull_request):
        _log.info('In-flight PR found: %s', pull_request.title)
        _log.info('%spull/%d', WPT_GH_URL, pull_request.number)

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

            change_id = self.wpt_github.extract_metadata('Change-Id: ', pull_request.body)
            if change_id:
                cl = GerritCL(data={'change_id': change_id}, api=self.gerrit)
                pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
                cl.post_comment((
                    'The WPT PR for this CL has been merged upstream! {pr_url}'
                ).format(
                    pr_url=pr_url
                ))

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

    def create_or_update_pull_request_from_cl(self, cl, pull_request=None):
        patch = cl.get_patch()
        updating = bool(pull_request)
        action_str = 'updating' if updating else 'creating'

        if self.local_wpt.test_patch(patch) == '':
            _log.error('Gerrit CL patch did not apply cleanly.')
            _log.error('First 500 characters of patch: %s', patch[0:500])
            return

        if self.dry_run:
            _log.info('[dry_run] Stopping before %s PR from CL', action_str)
            _log.info('\n\n[dry_run] subject:')
            _log.info(cl.subject)
            _log.debug('\n\n[dry_run] patch[0:500]:')
            _log.debug(patch[0:500])
            return

        message = cl.latest_commit_message_with_footers()

        # Annotate revision footer for Exporter's later use.
        message = '\n'.join([line for line in message.split('\n') if WPT_REVISION_FOOTER not in line])
        message += '\n{} {}'.format(WPT_REVISION_FOOTER, cl.current_revision_sha)

        branch_name = 'chromium-export-cl-{id}'.format(id=cl.change_id)
        self.local_wpt.create_branch_with_patch(branch_name, message, patch, cl.owner_email, force_push=True)

        if updating:
            response_data = self.wpt_github.update_pr(pull_request.number, cl.subject, message)
            _log.debug('Update PR response: %s', response_data)

            # TODO(jeffcarp): Turn PullRequest into a class with a .url method

            cl.post_comment((
                'Successfully updated WPT GitHub pull request with '
                'new revision "{subject}": {pr_url}'
            ).format(
                subject=cl.current_revision_description,
                pr_url='%spull/%d' % (WPT_GH_URL, pull_request.number),
            ))
        else:
            response_data = self.wpt_github.create_pr(branch_name, cl.subject, message)
            _log.debug('Create PR response: %s', response_data)

            data, status_code = self.wpt_github.add_label(response_data['number'])
            _log.info('Add label response (status %s): %s', status_code, data)

            cl.post_comment((
                'Exportable changes to web-platform-tests were detected in this CL '
                'and a pull request in the upstream repo has been made: {pr_url}.\n\n'
                'Travis CI has been kicked off and if it fails, we will let you know here. '
                'If this CL lands and Travis CI is green, we will auto-merge the PR.'
            ).format(
                pr_url='%spull/%d' % (WPT_GH_URL, response_data['number'])
            ))

        return response_data

    def corresponding_pull_request_for_commit(self, exportable_commit):
        """Search pull requests for one that corresponds to exportable_commit.

        Returns the pull_request if found, else returns None.
        """
        # Check for PRs created by commits on master.
        pull_request = self.wpt_github.pr_with_position(exportable_commit.position)
        if pull_request:
            return pull_request

        # Check for PRs created by open Gerrit CLs.
        change_id = exportable_commit.change_id()
        if change_id:
            return self.wpt_github.pr_with_change_id(change_id)

        return None

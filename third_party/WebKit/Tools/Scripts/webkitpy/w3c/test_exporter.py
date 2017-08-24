# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from webkitpy.w3c.common import (
    WPT_GH_URL,
    WPT_REVISION_FOOTER,
    EXPORT_PR_LABEL,
    PROVISIONAL_PR_LABEL
)
from webkitpy.w3c.gerrit import GerritAPI, GerritCL
from webkitpy.w3c.wpt_github import WPTGitHub, MergeError

_log = logging.getLogger(__name__)


class TestExporter(object):

    def __init__(self, host, gh_user, gh_token, gerrit_user, gerrit_token, dry_run=False):
        self.host = host
        self.wpt_github = WPTGitHub(host, gh_user, gh_token)

        self.gerrit = GerritAPI(self.host, gerrit_user, gerrit_token)

        self.dry_run = dry_run
        self.local_wpt = LocalWPT(self.host, gh_token)
        self.local_wpt.fetch()

    def run(self):
        """Creates PRs for in-flight CLs and merges changes that land on master.

        Returns:
            A boolean: True if success, False if there were any patch failures.
        """
        open_gerrit_cls = self.gerrit.query_exportable_open_cls()
        self.process_gerrit_cls(open_gerrit_cls)

        exportable_commits, errors = self.get_exportable_commits()
        for error in errors:
            _log.warn(error)
        self.process_chromium_commits(exportable_commits)

        return not bool(errors)

    def process_gerrit_cls(self, gerrit_cls):
        for cl in gerrit_cls:
            self.process_gerrit_cl(cl)

    def process_gerrit_cl(self, cl):
        _log.info('Found Gerrit in-flight CL: "%s" %s', cl.subject, cl.url)

        if not cl.has_review_started:
            _log.info('CL review has not started, skipping.')
            return

        pull_request = self.wpt_github.pr_with_change_id(cl.change_id)
        if pull_request:
            # If CL already has a corresponding PR, see if we need to update it.
            pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
            _log.info('In-flight PR found: %s', pr_url)

            pr_cl_revision = self.wpt_github.extract_metadata(WPT_REVISION_FOOTER + ' ', pull_request.body)
            if cl.current_revision_sha == pr_cl_revision:
                _log.info('PR revision matches CL revision. Nothing to do here.')
                return

            _log.info('New revision found, updating PR...')
            self.create_or_update_pull_request_from_cl(cl, pull_request)
        else:
            # Create a new PR for the CL if it does not have one.
            _log.info('No in-flight PR found for CL. Creating...')
            self.create_or_update_pull_request_from_cl(cl)

    def process_chromium_commits(self, exportable_commits):
        for commit in exportable_commits:
            self.process_chromium_commit(commit)

    def process_chromium_commit(self, commit):
        _log.info('Found exportable Chromium commit: %s %s', commit.subject(), commit.sha)

        pull_request = self.wpt_github.pr_for_chromium_commit(commit)
        if pull_request:
            pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
            _log.info('In-flight PR found: %s', pr_url)

            if pull_request.state != 'open':
                _log.info('Pull request is %s. Skipping.', pull_request.state)
                return

            if PROVISIONAL_PR_LABEL in pull_request.labels:
                # If the PR was created from a Gerrit in-flight CL, update the
                # PR with the final checked-in commit in Chromium history.
                # TODO(robertma): Only update the PR when it is not up-to-date
                # to avoid unnecessary Travis runs.
                _log.info('Updating PR with the final checked-in change...')
                self.create_or_update_pull_request_from_commit(commit, pull_request)
                self.remove_provisional_pr_label(pull_request)
                # Updating the patch triggers Travis, which will block merge.
                # Return early and merge next time.
                return

            self.merge_pull_request(pull_request)
        else:
            _log.info('No PR found for Chromium commit. Creating...')
            self.create_or_update_pull_request_from_commit(commit)

    def get_exportable_commits(self):
        return exportable_commits_over_last_n_commits(
            self.host, self.local_wpt, self.wpt_github)

    def remove_provisional_pr_label(self, pull_request):
        if self.dry_run:
            _log.info('[dry_run] Would have attempted to remove the provisional PR label')
            return

        _log.info('Removing provisional label "%s"...', PROVISIONAL_PR_LABEL)
        self.wpt_github.remove_label(pull_request.number, PROVISIONAL_PR_LABEL)

    def merge_pull_request(self, pull_request):
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

    def create_or_update_pull_request_from_commit(self, outbound_commit, pull_request=None):
        """Creates or updates a PR from a Chromium commit.

        Args:
            outbound_commit: A ChromiumCommit object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        patch = outbound_commit.format_patch()
        message = outbound_commit.message()
        subject = outbound_commit.subject()
        body = outbound_commit.body()
        author = outbound_commit.author()
        updating = bool(pull_request)
        action_str = 'updating' if updating else 'creating'

        if self.dry_run:
            _log.info('[dry_run] Stopping before %s PR from Chromium commit', action_str)
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.debug('\n\n[dry_run] patch[0:500]:')
            _log.debug(patch[0:500])
            return

        if updating:
            branch_name = self.wpt_github.get_pr_branch(pull_request.number)
        else:
            branch_name = 'chromium-export-{sha}'.format(sha=outbound_commit.short_sha)
        self.local_wpt.create_branch_with_patch(branch_name, message, patch, author, force_push=updating)

        if updating:
            response_data = self.wpt_github.update_pr(pull_request.number, subject, body)
            _log.info('Update PR response: %s', response_data)
        else:
            response_data = self.wpt_github.create_pr(branch_name, subject, body)
            _log.info('Create PR response: %s', response_data)

            if response_data:
                data, status_code = self.wpt_github.add_label(response_data['number'], EXPORT_PR_LABEL)
                _log.info('Add label response (status %s): %s', status_code, data)

        return response_data

    def create_or_update_pull_request_from_cl(self, cl, pull_request=None):
        """Creates or updates a PR from a Gerrit CL.

        Args:
            cl: A GerritCL object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        patch = cl.get_patch()
        message = cl.latest_commit_message_with_footers()
        author = cl.owner_email
        updating = bool(pull_request)
        action_str = 'updating' if updating else 'creating'

        success, error = self.local_wpt.test_patch(patch)
        if not success:
            _log.error('Gerrit CL patch did not apply cleanly:')
            _log.error(error)
            _log.error('First 500 characters of patch: << END_OF_PATCH_EXCERPT')
            _log.error(patch[0:500])
            _log.error('END_OF_PATCH_EXCERPT')
            return

        if self.dry_run:
            _log.info('[dry_run] Stopping before %s PR from CL', action_str)
            _log.info('\n\n[dry_run] subject:')
            _log.info(cl.subject)
            _log.debug('\n\n[dry_run] patch[0:500]:')
            _log.debug(patch[0:500])
            return

        # Annotate revision footer for Exporter's later use.
        message = '\n'.join([line for line in message.split('\n') if WPT_REVISION_FOOTER not in line])
        message += '\n{} {}'.format(WPT_REVISION_FOOTER, cl.current_revision_sha)

        branch_name = 'chromium-export-cl-{id}'.format(id=cl.change_id)
        self.local_wpt.create_branch_with_patch(branch_name, message, patch, author, force_push=updating)

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

            self.wpt_github.add_label(response_data['number'], EXPORT_PR_LABEL)
            self.wpt_github.add_label(response_data['number'], PROVISIONAL_PR_LABEL)

            cl.post_comment((
                'Exportable changes to web-platform-tests were detected in this CL '
                'and a pull request in the upstream repo has been made: {pr_url}.\n\n'
                'If this CL lands and Travis CI upstream is green, we will auto-merge the PR.\n\n'
                'Note: Please check the Travis CI status (at the bottom of the PR) '
                'before landing this CL and only land this CL if the status is green. '
                'Otherwise a human needs to step in and resolve it manually. '
                '(This may be automated in the future, see https://crbug.com/711447)\n\n'
                'WPT Export docs:\n'
                'https://chromium.googlesource.com/chromium/src/+/master'
                '/docs/testing/web_platform_tests.md#Automatic-export-process'
            ).format(
                pr_url='%spull/%d' % (WPT_GH_URL, response_data['number'])
            ))

        return response_data

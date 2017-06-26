# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import urllib2

from collections import namedtuple
from webkitpy.w3c.common import WPT_GH_ORG, WPT_GH_REPO_NAME
from webkitpy.common.memoized import memoized

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
EXPORT_LABEL = 'chromium-export'


class WPTGitHub(object):
    """An interface to GitHub for interacting with the web-platform-tests repo.

    This class contains methods for sending requests to the GitHub API.
    """

    def __init__(self, host, user=None, token=None, pr_history_window=100):
        self.host = host
        self.user = user
        self.token = token

        self._pr_history_window = pr_history_window

    def has_credentials(self):
        return self.user and self.token

    def auth_token(self):
        assert self.has_credentials()
        return base64.b64encode('{}:{}'.format(self.user, self.token))

    def request(self, path, method, body=None):
        assert path.startswith('/')

        if body:
            body = json.dumps(body)

        headers = {'Accept': 'application/vnd.github.v3+json'}

        if self.has_credentials():
            headers['Authorization'] = 'Basic {}'.format(self.auth_token())

        response = self.host.web.request(
            method=method,
            url=API_BASE + path,
            data=body,
            headers=headers
        )

        status_code = response.getcode()
        try:
            return json.load(response), status_code
        except ValueError:
            return None, status_code

    def create_pr(self, remote_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            A raw response object if successful, None if not.
        """
        assert remote_branch_name
        assert desc_title
        assert body

        path = '/repos/%s/%s/pulls' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
        body = {
            'title': desc_title,
            'body': body,
            'head': remote_branch_name,
            'base': 'master',
        }
        data, status_code = self.request(path, method='POST', body=body)

        if status_code != 201:
            return None

        return data

    def update_pr(self, pr_number, desc_title, body):
        """Updates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#update-a-pull-request

        Returns:
            A raw response object if successful, None if not.
        """
        path = '/repos/{}/{}/pulls/{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        body = {
            'title': desc_title,
            'body': body,
        }
        data, status_code = self.request(path, method='PATCH', body=body)

        if status_code != 201:
            return None

        return data

    def add_label(self, number, label=EXPORT_LABEL):
        path = '/repos/%s/%s/issues/%d/labels' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number
        )
        body = [label]
        return self.request(path, method='POST', body=body)

    def in_flight_pull_requests(self):
        path = '/search/issues?q=repo:{}/{}%20is:open%20type:pr%20label:{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            EXPORT_LABEL
        )
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return [self.make_pr_from_item(item) for item in data['items']]
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def make_pr_from_item(self, item):
        return PullRequest(
            title=item['title'],
            number=item['number'],
            body=item['body'],
            state=item['state'])

    @memoized
    def all_pull_requests(self):
        # TODO(jeffcarp): Add pagination to fetch >99 PRs
        assert self._pr_history_window <= 100, 'Maximum GitHub page size exceeded.'
        path = (
            '/search/issues'
            '?q=repo:{}/{}%20type:pr%20label:{}'
            '&page=1'
            '&per_page={}'
        ).format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            EXPORT_LABEL,
            self._pr_history_window
        )

        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            for item in data['items']:
                print item['state'], item['number']
            return [self.make_pr_from_item(item) for item in data['items']]
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def get_pr_branch(self, pr_number):
        path = '/repos/{}/{}/pulls/{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return data['head']['ref']
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def merge_pull_request(self, pull_request_number):
        path = '/repos/%s/%s/pulls/%d/merge' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pull_request_number
        )
        body = {
            # This currently will noop because the feature is in an opt-in beta.
            # Once it leaves beta this will start working.
            'merge_method': 'rebase',
        }

        try:
            data, status_code = self.request(path, method='PUT', body=body)
        except urllib2.HTTPError as e:
            if e.code == 405:
                raise MergeError()
            else:
                raise

        if status_code != 200:
            raise Exception('Received non-200 status code (%d) while merging PR #%d' % (status_code, pull_request_number))

        return data

    def delete_remote_branch(self, remote_branch_name):
        # TODO(jeffcarp): Unit test this method
        path = '/repos/%s/%s/git/refs/heads/%s' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            remote_branch_name
        )
        data, status_code = self.request(path, method='DELETE')

        if status_code != 204:
            # TODO(jeffcarp): Raise more specific exception (create MergeError class?)
            raise Exception('Received non-204 status code attempting to delete remote branch: {}'.format(status_code))

        return data

    def pr_with_change_id(self, target_change_id):
        for pull_request in self.all_pull_requests():
            change_id = self.extract_metadata('Change-Id: ', pull_request.body)
            if change_id == target_change_id:
                return pull_request
        return None

    def pr_with_position(self, position):
        for pull_request in self.all_pull_requests():
            pr_commit_position = self.extract_metadata('Cr-Commit-Position: ', pull_request.body)
            if position == pr_commit_position:
                return pull_request
        return None

    def extract_metadata(self, tag, commit_body):
        for line in commit_body.splitlines():
            if line.startswith(tag):
                return line[len(tag):]
        return None


class MergeError(Exception):
    """An error specifically for when a PR cannot be merged.

    This should only be thrown when GitHub returns status code 405,
    indicating that the PR could not be merged.
    """
    pass


PullRequest = namedtuple('PullRequest', ['title', 'number', 'body', 'state'])

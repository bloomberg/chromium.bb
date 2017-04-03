# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import urllib2

from collections import namedtuple


_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
EXPORT_LABEL = 'chromium-export'


class WPTGitHub(object):

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token
        assert self.user and self.token

    def auth_token(self):
        return base64.b64encode('{}:{}'.format(self.user, self.token))

    def request(self, path, method, body=None):
        assert path.startswith('/')

        if body:
            body = json.dumps(body)

        response = self.host.web.request(
            method=method,
            url=API_BASE + path,
            data=body,
            headers={
                'Accept': 'application/vnd.github.v3+json',
                'Authorization': 'Basic {}'.format(self.auth_token()),
            },
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

        path = '/repos/w3c/web-platform-tests/pulls'
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

    def add_label(self, number):
        path = '/repos/w3c/web-platform-tests/issues/%d/labels' % number
        body = [EXPORT_LABEL]
        return self.request(path, method='POST', body=body)

    def in_flight_pull_requests(self):
        path = '/search/issues?q=repo:w3c/web-platform-tests%20is:open%20type:pr%20label:{}'.format(EXPORT_LABEL)
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

    def all_pull_requests(self, limit=30):
        assert limit <= 100, 'Maximum GitHub page size exceeded.'
        path = '/search/issues?q=repo:w3c/web-platform-tests%20type:pr%20label:{}&page=1&per_page={}'.format(EXPORT_LABEL, limit)
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return [self.make_pr_from_item(item) for item in data['items']]
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def get_pr_branch(self, pr_number):
        path = '/repos/w3c/web-platform-tests/pulls/{}'.format(pr_number)
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return data['head']['ref']
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def merge_pull_request(self, pull_request_number):
        path = '/repos/w3c/web-platform-tests/pulls/%d/merge' % pull_request_number
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
        path = '/repos/w3c/web-platform-tests/git/refs/heads/%s' % remote_branch_name
        data, status_code = self.request(path, method='DELETE')

        if status_code != 204:
            # TODO(jeffcarp): Raise more specific exception (create MergeError class?)
            raise Exception('Received non-204 status code attempting to delete remote branch: {}'.format(status_code))

        return data


class MergeError(Exception):
    """An error specifically for when a PR cannot be merged.

    This should only be thrown when GitHub returns status code 405,
    indicating that the PR could not be merged.
    """
    pass

PullRequest = namedtuple('PullRequest', ['title', 'number', 'body', 'state'])

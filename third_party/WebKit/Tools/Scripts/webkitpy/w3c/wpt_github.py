# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import re
import urllib2
from collections import namedtuple

from webkitpy.common.memoized import memoized
from webkitpy.w3c.common import WPT_GH_ORG, WPT_GH_REPO_NAME, EXPORT_PR_LABEL

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
MAX_PER_PAGE = 100


class WPTGitHub(object):
    """An interface to GitHub for interacting with the web-platform-tests repo.

    This class contains methods for sending requests to the GitHub API.
    """

    def __init__(self, host, user=None, token=None, pr_history_window=5000):
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
        """Sends a request to GitHub API and deserializes the response.

        Args:
            path: API endpoint without base URL (starting with '/').
            method: HTTP method to be used for this request.
            body: Optional payload in the request body (default=None).

        Returns:
            A JSONResponse instance.
        """
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
        return JSONResponse(response)

    def extract_link_next(self, link_header):
        """Extracts the URI to the next page of results from a response.

        As per GitHub API specs, the link to the next page of results is
        extracted from the Link header -- the link with relation type "next".
        Docs: https://developer.github.com/v3/#pagination (and RFC 5988)

        Args:
            link_header: The value of the Link header in responses from GitHub.

        Returns:
            Path to the next page (without base URL), or None if not found.
        """
        # TODO(robertma): Investigate "may require expansion as URI templates" mentioned in docs.
        # Example Link header:
        # <https://api.github.com/resources?page=3>; rel="next", <https://api.github.com/resources?page=50>; rel="last"
        if link_header is None:
            return None
        link_re = re.compile(r'<(.+?)>; *rel="(.+?)"')
        match = link_re.search(link_header)
        while match:
            link, rel = match.groups()
            if rel.lower() == 'next':
                # Strip API_BASE so that the return value is useful for request().
                assert link.startswith(API_BASE)
                return link[len(API_BASE):]
            match = link_re.search(link_header, match.end())
        return None

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
        response = self.request(path, method='POST', body=body)

        if response.status_code != 201:
            return None

        return response.data

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
        response = self.request(path, method='PATCH', body=body)

        if response.status_code != 201:
            return None

        return response.data

    def add_label(self, number, label):
        path = '/repos/%s/%s/issues/%d/labels' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number
        )
        body = [label]
        response = self.request(path, method='POST', body=body)
        return response.data, response.status_code

    def remove_label(self, number, label):
        path = '/repos/%s/%s/issues/%d/labels/%s' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number,
            urllib2.quote(label),
        )

        response = self.request(path, method='DELETE')
        # The GitHub API documentation claims that this endpoint returns a 204
        # on success. However in reality it returns a 200.
        # https://developer.github.com/v3/issues/labels/#remove-a-label-from-an-issue
        if response.status_code not in (200, 204):
            raise GitHubError('Received non-200 status code attempting to delete label: {}'.format(response.status_code))

    def make_pr_from_item(self, item):
        labels = [label['name'] for label in item['labels']]
        return PullRequest(
            title=item['title'],
            number=item['number'],
            body=item['body'],
            state=item['state'],
            labels=labels)

    @memoized
    def all_pull_requests(self):
        path = (
            '/search/issues'
            '?q=repo:{}/{}%20type:pr%20label:{}'
            '&page=1'
            '&per_page={}'
        ).format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            EXPORT_PR_LABEL,
            min(MAX_PER_PAGE, self._pr_history_window)
        )
        all_prs = []
        while path is not None and len(all_prs) < self._pr_history_window:
            response = self.request(path, method='GET')
            if response.status_code == 200:
                if response.data['incomplete_results']:
                    raise GitHubError('Received incomplete results when fetching all pull requests. Data received:\n%s'
                                      % response.data)
                prs = [self.make_pr_from_item(item) for item in response.data['items']]
                all_prs += prs[:self._pr_history_window - len(all_prs)]
            else:
                raise GitHubError('Received non-200 status code (%d) when fetching all pull requests: %s'
                                  % (response.status_code, path))
            path = self.extract_link_next(response.getheader('Link'))
        return all_prs

    def get_pr_branch(self, pr_number):
        path = '/repos/{}/{}/pulls/{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        response = self.request(path, method='GET')
        if response.status_code == 200:
            return response.data['head']['ref']
        else:
            raise Exception('Non-200 status code (%s): %s' % (response.status_code, response.data))

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
            response = self.request(path, method='PUT', body=body)
        except urllib2.HTTPError as e:
            if e.code == 405:
                raise MergeError()
            else:
                raise

        if response.status_code != 200:
            raise Exception('Received non-200 status code (%d) while merging PR #%d' % (response.status_code, pull_request_number))

        return response.data

    def delete_remote_branch(self, remote_branch_name):
        # TODO(jeffcarp): Unit test this method
        path = '/repos/%s/%s/git/refs/heads/%s' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            remote_branch_name
        )
        response = self.request(path, method='DELETE')

        if response.status_code != 204:
            raise GitHubError('Received non-204 status code attempting to delete remote branch: {}'.format(response.status_code))

        return response.data

    def pr_for_chromium_commit(self, chromium_commit):
        """Returns a PR corresponding to the given ChromiumCommit, or None."""
        pull_request = self.pr_with_change_id(chromium_commit.change_id())
        if pull_request:
            return pull_request
        # The Change ID can't be used for commits made via Rietveld,
        # so we fall back to trying to use commit position here.
        # Note that Gerrit returns ToT+1 as the commit positions for in-flight
        # CLs, but they are scrubbed from the PR description and hence would
        # not be mismatched to random Chromium commits in the fallback.
        # TODO(robertma): Remove this fallback after Rietveld becomes read-only.
        return self.pr_with_position(chromium_commit.position)

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


class JSONResponse(object):
    """An HTTP response containing JSON data."""

    def __init__(self, raw_response):
        """Initializes a JSONResponse instance.

        Args:
            raw_response: a response object returned by open methods in urllib2.
        """
        self._raw_response = raw_response
        self.status_code = raw_response.getcode()
        try:
            self.data = json.load(raw_response)
        except ValueError:
            self.data = None

    def getheader(self, header):
        """Gets the value of the header with the given name.

        Delegates to HTTPMessage.getheader(), which is case-insensitive."""
        return self._raw_response.info().getheader(header)


class MergeError(Exception):
    """An error specifically for when a PR cannot be merged.

    This should only be thrown when GitHub returns status code 405,
    indicating that the PR could not be merged.
    """
    pass


class GitHubError(Exception):
    """Raised when an GitHub returns a non-OK response status for a request."""
    pass


PullRequest = namedtuple('PullRequest', ['title', 'number', 'body', 'state', 'labels'])

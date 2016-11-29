# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import httplib2
import logging

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
EXPORT_LABEL = 'chromium-export'


class WPTGitHub(object):

    def __init__(self, host):
        self.host = host
        self.user = self.host.environ.get('GH_USER')
        self.token = self.host.environ.get('GH_TOKEN')

        assert self.user and self.token, 'must have GH_USER and GH_TOKEN env vars'

    def auth_token(self):
        return base64.encodestring('{}:{}'.format(self.user, self.token))

    def create_pr(self, local_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            A raw response object if successful, None if not.
        """
        assert local_branch_name
        assert desc_title
        assert body

        pr_branch_name = '{}:{}'.format(self.user, local_branch_name)

        # TODO(jeffcarp): CC foolip and qyearsley on all PRs for now
        # TODO(jeffcarp): add HTTP to Host and use that here
        conn = httplib2.Http()
        headers = {
            "Accept": "application/vnd.github.v3+json",
            "Authorization": "Basic " + self.auth_token()
        }
        body = {
            "title": desc_title,
            "body": body,
            "head": pr_branch_name,
            "base": 'master',
            "labels": [EXPORT_LABEL]
        }
        resp, content = conn.request("https://api.github.com/repos/w3c/web-platform-tests/pulls",
                                     "POST", body=json.JSONEncoder().encode(body), headers=headers)
        _log.info("GitHub response: %s", content)
        if resp["status"] != "201":
            return None
        return json.loads(content)

    def in_flight_pull_requests(self):
        url_encoded_label = EXPORT_LABEL.replace(' ', '%20')
        path = '/search/issues?q=repo:w3c/web-platform-tests%20is:open%20type:pr%20labels:{}'.format(url_encoded_label)
        response, content = self.request(path)
        if response['status'] == '200':
            data = json.loads(content)
            return data['items']
        else:
            raise Exception('Non-200 status code (%s): %s' % (response['status'], content))

    def request(self, path, body=None):
        assert path.startswith('/')

        # Not used yet since only hitting public API
        # headers = {
        #     "Accept": "application/vnd.github.v3+json",
        #     "Authorization": "Basic " + self.auth_token()
        # }

        if body:
            json_body = json.dumps(body)
        else:
            json_body = None

        conn = httplib2.Http()
        return conn.request(API_BASE + path, body=json_body)

    def merge_pull_request(self, pull_request_number):
        path = '/repos/w3c/web-platform-tests/pulls/%d/merge' % pull_request_number
        body = {}
        response, content = self.request(path, body)

        if response['status'] == '200':
            return json.loads(content)
        else:
            raise Exception('PR could not be merged: %d' % pull_request_number)

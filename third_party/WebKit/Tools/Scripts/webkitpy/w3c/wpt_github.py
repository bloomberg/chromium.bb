# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import os
import sys
import urllib2

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder


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
        return base64.encodestring('{}:{}'.format(self.user, self.token)).strip()

    def request(self, path, method, body=None):
        assert path.startswith('/')
        if body:
            body = json.dumps(body)
        opener = urllib2.build_opener(urllib2.HTTPHandler)
        request = urllib2.Request(url=API_BASE + path, data=body)
        request.add_header('Accept', 'application/vnd.github.v3+json')
        request.add_header('Authorization', 'Basic {}'.format(self.auth_token()))
        request.get_method = lambda: method
        response = opener.open(request)
        status_code = response.getcode()
        return json.load(response), status_code

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
        path = '/repos/w3c/web-platform-tests/pulls'
        body = {
            "title": desc_title,
            "body": body,
            "head": pr_branch_name,
            "base": 'master',
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
        url_encoded_label = EXPORT_LABEL.replace(' ', '%20')
        path = '/search/issues?q=repo:w3c/web-platform-tests%20is:open%20type:pr%20labels:{}'.format(url_encoded_label)
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return data['items']
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def merge_pull_request(self, pull_request_number):
        path = '/repos/w3c/web-platform-tests/pulls/%d/merge' % pull_request_number
        body = {}
        response, content = self.request(path, method='PUT', body=body)

        if response['status'] == '200':
            return json.loads(content)
        else:
            raise Exception('PR could not be merged: %d' % pull_request_number)

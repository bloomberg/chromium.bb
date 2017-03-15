# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.w3c.wpt_github import MergeError


class MockWPTGitHub(object):

    def __init__(self, pull_requests, unsuccessful_merge_index=-1):
        self.pull_requests = pull_requests
        self.calls = []
        self.pull_requests_created = []
        self.pull_requests_merged = []
        self.unsuccessful_merge_index = unsuccessful_merge_index

    def in_flight_pull_requests(self):
        self.calls.append('in_flight_pull_requests')
        return self.pull_requests

    def merge_pull_request(self, number):
        self.calls.append('merge_pull_request')

        for index, pr in enumerate(self.pull_requests):
            if pr.number == number and index == self.unsuccessful_merge_index:
                raise MergeError()

        self.pull_requests_merged.append(number)

    def create_pr(self, remote_branch_name, desc_title, body):
        self.calls.append('create_pr')

        assert remote_branch_name
        assert desc_title
        assert body

        self.pull_requests_created.append((remote_branch_name, desc_title, body))

        return {}

    def delete_remote_branch(self, _):
        self.calls.append('delete_remote_branch')

    def get_pr_branch(self, number):
        self.calls.append('get_pr_branch')
        return 'fake branch for PR {}'.format(number)

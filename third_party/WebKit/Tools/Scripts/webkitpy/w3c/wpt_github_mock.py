# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.w3c.wpt_github import MergeError


class MockWPTGitHub(object):

    def __init__(self, pull_requests, unsuccessful_merge_index=-1, create_pr_fail_index=-1):
        self.pull_requests = pull_requests
        self.calls = []
        self.pull_requests_created = []
        self.pull_requests_merged = []
        self.unsuccessful_merge_index = unsuccessful_merge_index
        self.create_pr_index = 0
        self.create_pr_fail_index = create_pr_fail_index

    def all_pull_requests(self, limit=30):  # pylint: disable=unused-argument
        self.calls.append('all_pull_requests')
        return self.pull_requests

    def merge_pull_request(self, number):
        self.calls.append('merge_pull_request')

        for index, pr in enumerate(self.pull_requests):
            if pr.number == number and index == self.unsuccessful_merge_index:
                raise MergeError()

        self.pull_requests_merged.append(number)

    def create_pr(self, remote_branch_name, desc_title, body):
        self.calls.append('create_pr')

        if self.create_pr_fail_index != self.create_pr_index:
            self.pull_requests_created.append((remote_branch_name, desc_title, body))

        self.create_pr_index += 1

        return {'number': 5678}

    def update_pr(self, pr_number, desc_title, body):  # pylint: disable=unused-argument
        self.calls.append('update_pr')

        return {'number': 5678}

    def delete_remote_branch(self, _):
        self.calls.append('delete_remote_branch')

    def add_label(self, _):
        self.calls.append('add_label')
        return {}, 200

    def get_pr_branch(self, number):
        self.calls.append('get_pr_branch')
        return 'fake branch for PR {}'.format(number)

    def pr_with_position(self, position):
        self.calls.append('pr_with_position')
        for pr in self.pull_requests:
            if position in pr.body:
                return pr

    def pr_with_change_id(self, change_id):
        self.calls.append('pr_with_change_id')
        for pr in self.pull_requests:
            if change_id in pr.body:
                return pr

    def extract_metadata(self, tag, commit_body):
        for line in commit_body.splitlines():
            if line.startswith(tag):
                return line[len(tag):]
        return None

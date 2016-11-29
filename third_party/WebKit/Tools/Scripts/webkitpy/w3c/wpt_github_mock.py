class MockWPTGitHub(object):

    def __init__(self, pull_requests, unsuccessful_merge=False):
        self.pull_requests = pull_requests
        self.unsuccessful_merge = unsuccessful_merge
        self.calls = []
        self.pull_requests_created = []

    def in_flight_pull_requests(self):
        self.calls.append('in_flight_pull_requests')
        return self.pull_requests

    def merge_pull_request(self, number):
        self.calls.append('merge_pull_request')
        if self.unsuccessful_merge:
            raise Exception('PR could not be merged: %d' % number)

    def create_pr(self, local_branch_name, desc_title, body):
        self.calls.append('create_pr')

        assert local_branch_name
        assert desc_title
        assert body

        self.pull_requests_created.append((local_branch_name, desc_title, body))

        return {}

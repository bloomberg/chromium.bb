# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Rietveld, Gerrit,
and Buildbucket to manage changelists and try jobs associated with them.
"""

import time


class GitCL(object):

    def __init__(self, executive, auth_refresh_token_json=None, cwd=None):
        self._executive = executive
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output."""
        command = ['git', 'cl'] + args
        if self._auth_refresh_token_json:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        return self._executive.run_command(command, cwd=self._cwd)

    def get_issue_number(self):
        return self.run(['issue']).split()[2]

    def has_failing_try_results(self, poll_delay_seconds=300):
        """Waits for try job results and checks whether there are failing results."""
        # TODO(qyearsley): Refactor to make this more easily-testable.
        # TODO(qyearsley): Add a time-out to avoid infinite looping.
        while True:
            time.sleep(poll_delay_seconds)
            print 'Waiting for results.'
            out = self.run(['try-results'])
            results = self.parse_try_job_results(out)
            if results.get('Started') or results.get('Scheduled'):
                continue
            if results.get('Failures'):
                return True
            return False

    @staticmethod
    def parse_try_job_results(results):
        """Parses try job results from `git cl try-results`.

        Args:
            results: The stdout obtained by running `git cl try-results`.

        Returns:
            A dict mapping result type (e.g. Success, Failure) to list of bots
            with that result type. The list of builders is represented as a set
            and any bots with both success and failure results are not included
            in failures.
        """
        sets = {}
        for line in results.splitlines():
            line = line.strip()
            if line[-1] == ':':
                result_type = line[:-1]
                sets[result_type] = set()
            elif line.split()[0] == 'Total:':
                break
            else:
                sets[result_type].add(line.split()[0])
        return sets

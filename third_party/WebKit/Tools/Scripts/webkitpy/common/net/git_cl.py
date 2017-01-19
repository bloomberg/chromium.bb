# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Rietveld, Gerrit,
and Buildbucket to manage changelists and try jobs associated with them.
"""

import json
import logging

_log = logging.getLogger(__name__)

_COMMANDS_THAT_REQUIRE_AUTH = (
    'archive', 'comments', 'commit', 'description', 'diff', 'land', 'lint', 'owners', 'patch',
    'presubmit', 'set-close', 'set-commit', 'status', 'try-results', 'try', 'upload',
)

class GitCL(object):

    def __init__(self, host, auth_refresh_token_json=None, cwd=None):
        self._host = host
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output."""
        command = ['git', 'cl'] + args
        if self._auth_refresh_token_json and args[0] in _COMMANDS_THAT_REQUIRE_AUTH:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        return self._host.executive.run_command(command, cwd=self._cwd)

    def get_issue_number(self):
        return self.run(['issue']).split()[2]

    def wait_for_try_jobs(self, poll_delay_seconds=10 * 60, timeout_seconds=120 * 60):
        """Waits until all try jobs are finished.

        Args:
            poll_delay_seconds: Time to wait between fetching results.
            timeout_seconds: Time to wait before aborting.

        Returns:
            A list of try job result dicts, or None if a timeout occurred.
        """
        start = self._host.time()
        self._host.print_('Waiting for try jobs (timeout: %d s, poll interval %d s).' %
                          (timeout_seconds, poll_delay_seconds))
        while self._host.time() - start < timeout_seconds:
            self._host.sleep(poll_delay_seconds)
            try_results = self.fetch_try_results()
            _log.debug('Fetched try results: %s', try_results)
            if self.all_jobs_finished(try_results):
                self._host.print_()
                self._host.print_('All jobs finished.')
                return try_results
            self._host.print_('.', end='')
            self._host.sleep(poll_delay_seconds)
        self._host.print_()
        self._host.print_('Timed out waiting for try results.')
        return None

    def fetch_try_results(self):
        """Requests results of try jobs for the current CL."""
        with self._host.filesystem.mkdtemp() as temp_directory:
            results_path = self._host.filesystem.join(temp_directory, 'try-results.json')
            self.run(['try-results', '--json', results_path])
            contents = self._host.filesystem.read_text_file(results_path)
            _log.debug('Fetched try results to file "%s".', results_path)
            self._host.filesystem.remove(results_path)
        return json.loads(contents)

    @staticmethod
    def all_jobs_finished(try_results):
        return all(r.get('status') == 'COMPLETED' for r in try_results)

    @staticmethod
    def has_failing_try_results(try_results):
        return any(r.get('result') == 'FAILURE' for r in try_results)

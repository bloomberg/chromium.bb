# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Rietveld, Gerrit,
and Buildbucket to manage changelists and try jobs associated with them.
"""

import collections
import json
import logging
import re

from webkitpy.common.net.buildbot import Build, filter_latest_builds
from webkitpy.common.checkout.git import Git

_log = logging.getLogger(__name__)

# A refresh token may be needed for some commands, such as git cl try,
# in order to authenticate with buildbucket.
_COMMANDS_THAT_TAKE_REFRESH_TOKEN = ('try',)


class TryJobStatus(collections.namedtuple('TryJobStatus', ('status', 'result'))):
    """Represents a current status of a particular job.

    Specifically, whether it is scheduled or started or finished, and if
    it is finished, whether it failed or succeeded. If it failed,
    """
    def __new__(cls, status, result=None):
        assert status in ('SCHEDULED', 'STARTED', 'COMPLETED')
        assert result in (None, 'FAILURE', 'SUCCESS', 'CANCELED')
        return super(TryJobStatus, cls).__new__(cls, status, result)


class GitCL(object):

    def __init__(self, host, auth_refresh_token_json=None, cwd=None):
        self._host = host
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd
        self._git_executable_name = Git.find_executable_name(host.executive, host.platform)

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output."""
        command = [self._git_executable_name, 'cl'] + args
        if self._auth_refresh_token_json and args[0] in _COMMANDS_THAT_TAKE_REFRESH_TOKEN:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        return self._host.executive.run_command(command, cwd=self._cwd)

    def trigger_try_jobs(self, builders):
        # This method assumes the bots to be triggered are Blink try bots,
        # which are all on the master tryserver.blink except android_blink_rel.
        if 'android_blink_rel' in builders:
            self.run(['try', '-b', 'android_blink_rel'])
            builders.remove('android_blink_rel')
        # TODO(qyearsley): Stop explicitly adding the master name when
        # git cl try can get the master name; see http://crbug.com/700523.
        command = ['try', '-m', 'tryserver.blink']
        for builder in sorted(builders):
            command.extend(['-b', builder])
        self.run(command)

    def get_issue_number(self):
        return self.run(['issue']).split()[2]

    def wait_for_try_jobs(self, poll_delay_seconds=10 * 60, timeout_seconds=120 * 60):
        """Waits until all try jobs are finished.

        Args:
            poll_delay_seconds: Time to wait between fetching results.
            timeout_seconds: Time to wait before aborting.

        Returns:
            A dict mapping Build objects to TryJobStatus objects, or
            None if a timeout occurred.
        """
        start = self._host.time()
        self._host.print_('Waiting for try jobs (timeout: %d seconds).' % timeout_seconds)
        while self._host.time() - start < timeout_seconds:
            self._host.sleep(poll_delay_seconds)
            try_results = self.try_job_results()
            _log.debug('Fetched try results: %s', try_results)
            if try_results and self.all_finished(try_results):
                self._host.print_('All jobs finished.')
                return try_results
            self._host.print_('Waiting. %d seconds passed.' % (self._host.time() - start))
            self._host.sleep(poll_delay_seconds)
        self._host.print_('Timed out waiting for try results.')
        return None

    def latest_try_jobs(self, builder_names=None):
        """Fetches a dict of Build to TryJobStatus for the latest try jobs.

        This includes jobs that are not yet finished and builds with infra
        failures, so if a build is in this list, that doesn't guarantee that
        there are results.

        Args:
            builder_names: Optional list of builders used to filter results.

        Returns:
            A dict mapping Build objects to TryJobStatus objects, with
            only the latest jobs included.
        """
        return self.filter_latest(self.try_job_results(builder_names))

    @staticmethod
    def filter_latest(try_results):
        """Returns the latest entries from from a Build to TryJobStatus dict."""
        if try_results is None:
            return None
        latest_builds = filter_latest_builds(try_results.keys())
        return {b: s for b, s in try_results.items() if b in latest_builds}

    def try_job_results(self, builder_names=None):
        """Returns a dict mapping Build objects to TryJobStatus objects."""
        raw_results = self.fetch_raw_try_job_results()
        build_to_status = {}
        for result in raw_results:
            if builder_names and result['builder_name'] not in builder_names:
                continue
            build_to_status[self._build(result)] = self._try_job_status(result)
        return build_to_status

    def fetch_raw_try_job_results(self):
        """Requests results of try jobs for the current CL and the parsed JSON.

        The return value is expected to be a list of dicts, which each are
        expected to have the fields "builder_name", "status", "result", and
        "url". The format is determined by the output of "git cl try-results".
        """
        with self._host.filesystem.mkdtemp() as temp_directory:
            results_path = self._host.filesystem.join(temp_directory, 'try-results.json')
            self.run(['try-results', '--json', results_path])
            contents = self._host.filesystem.read_text_file(results_path)
            _log.debug('Fetched try results to file "%s".', results_path)
            self._host.filesystem.remove(results_path)
        return json.loads(contents)

    @staticmethod
    def _build(result_dict):
        """Converts a parsed try result dict to a Build object."""
        builder_name = result_dict['builder_name']
        url = result_dict['url']
        if url is None:
            return Build(builder_name, None)
        match = re.match(r'.*/builds/(\d+)/?$', url)
        if match:
            build_number = match.group(1)
            return Build(builder_name, int(build_number))
        match = re.match(r'.*/task/([0-9a-f]+)/?$', url)
        assert match, '%s did not match expected format' % url
        task_id = match.group(1)
        return Build(builder_name, task_id)

    @staticmethod
    def _try_job_status(result_dict):
        """Converts a parsed try result dict to a TryJobStatus object."""
        return TryJobStatus(result_dict['status'], result_dict['result'])

    @staticmethod
    def all_finished(try_results):
        return all(s.status == 'COMPLETED' for s in try_results.values())

    @staticmethod
    def all_success(try_results):
        return all(s.status == 'COMPLETED' and s.result == 'SUCCESS'
                   for s in try_results.values())

    @staticmethod
    def some_failed(try_results):
        return any(s.result == 'FAILURE' for s in try_results.values())

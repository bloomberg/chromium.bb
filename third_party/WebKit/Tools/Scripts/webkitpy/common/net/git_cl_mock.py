# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.common.net.buildbot import filter_latest_builds


# TODO(qyearsley): Use this class in wpt_expectations_updater_unittest and rebaseline_cl_unittest.
class MockGitCL(object):

    def __init__(self, host, results=None):
        self._host = host
        self._results = results or {}
        self.calls = []

    def run(self, args):
        self.calls.append(['git', 'cl'] + args)
        return 'mock output'

    def trigger_try_jobs(self, builders=None):
        builders = builders or self._host.builders.all_try_builder_names()
        command = ['try', '-m', 'tryserver.blink']
        for builder in sorted(builders):
            command.extend(['-b', builder])
        self.run(command)

    def get_issue_number(self):
        return '1234'

    def wait_for_try_jobs(self, **_):
        return self._results

    def latest_try_jobs(self, **_):
        latest_builds = filter_latest_builds(self._results)
        return {b: s for b, s in self._results.items() if b in latest_builds}

    def try_job_results(self, **_):
        return self._results

    @staticmethod
    def all_jobs_finished(results):
        return all(s.status == 'COMPLETED' for s in results.values())

    @staticmethod
    def has_failing_try_results(results):
        return any(s.result == 'FAILURE' for s in results.values())

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.common.net.git_cl import GitCL

# pylint: disable=unused-argument

class MockGitCL(object):

    def __init__(self, host, results=None, issue_number='1234'):
        self._host = host
        self._results = results or {}
        self._issue_number = issue_number
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
        return self._issue_number

    def try_job_results(self, **_):
        return self._results

    def wait_for_try_jobs(self, **_):
        return self._results

    def latest_try_jobs(self, builder_names=None):
        return self.filter_latest(self._results)

    @staticmethod
    def filter_latest(try_results):
        return GitCL.filter_latest(try_results)

    @staticmethod
    def all_finished(try_results):
        return GitCL.all_finished(try_results)

    @staticmethod
    def all_success(try_results):
        return GitCL.all_success(try_results)

    @staticmethod
    def some_failed(try_results):
        return GitCL.some_failed(try_results)

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for a Rietveld issue.

This command interacts with the Rietveld API to get information about try jobs
with layout test results.
"""

import logging
import optparse

from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.rietveld import latest_try_jobs, changed_files
from webkitpy.common.net.web import Web
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import BASELINE_SUFFIX_LIST
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand


_log = logging.getLogger(__name__)


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = "rebaseline-cl"
    help_text = "Fetches new baselines for a CL from test runs on try bots."
    long_help = ("By default, this command will check the latest try results "
                 "and download new baselines for any tests that have been "
                 "changed in the given CL that have failed and have new "
                 "baselines. After downloading, the baselines for different "
                 "platforms should be optimized (conslidated).")
    show_in_main_help = True

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            optparse.make_option(
                '--issue', type='int', default=None,
                help='Rietveld issue number; if none given, this will be obtained via `git cl issue`.'),
            optparse.make_option(
                '--dry-run', action='store_true', default=False,
                help='Dry run mode; list actions that would be performed but do not do anything.'),
            optparse.make_option(
                '--only-changed-tests', action='store_true', default=False,
                help='Only download new baselines for tests that are changed in the CL.'),
            self.no_optimize_option,
            self.results_directory_option,
        ])
        self.web = Web()

    def execute(self, options, args, tool):
        issue_number = self._get_issue_number(options)
        if not issue_number:
            return
        if args:
            test_prefix_list = {}
            try_jobs = latest_try_jobs(issue_number, self._try_bots(), self.web)
            builds = [Build(j.builder_name, j.build_number) for j in try_jobs]
            for test in args:
                test_prefix_list[test] = {b: BASELINE_SUFFIX_LIST for b in builds}
        else:
            test_prefix_list = self._test_prefix_list(
                issue_number, only_changed_tests=options.only_changed_tests)
        self._log_test_prefix_list(test_prefix_list)

        if options.dry_run:
            return
        self._rebaseline(options, test_prefix_list, update_scm=False)

    def _get_issue_number(self, options):
        """Gets the Rietveld CL number from either |options| or from the current local branch."""
        if options.issue:
            return options.issue
        issue_number = self.git().get_issue_number()
        _log.debug('Issue number for current branch: %s', issue_number)
        if not issue_number.isdigit():
            _log.error('No issue number given and no issue for current branch.')
            return None
        return int(issue_number)

    def git(self):
        """Returns a Git instance; can be overridden for tests."""
        # Pass in a current working directory inside of the repo so
        # that this command can be called from outside of the repo.
        return Git(cwd=self._tool.filesystem.dirname(self._tool.path()))

    def _test_prefix_list(self, issue_number, only_changed_tests):
        """Returns a collection of test, builder and file extensions to get new baselines for.

        Args:
            issue_number: The CL number of the change which needs new baselines.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A dict containing information about which new baselines to download.
        """
        builds_to_tests = self._builds_to_tests(issue_number)
        if only_changed_tests:
            files_in_cl = changed_files(issue_number, self.web)
            finder = WebKitFinder(self._tool.filesystem)
            tests_in_cl = [finder.layout_test_name(f) for f in files_in_cl]
        result = {}
        for build, tests in builds_to_tests.iteritems():
            for test in tests:
                if only_changed_tests and test not in tests_in_cl:
                    continue
                if test not in result:
                    result[test] = {}
                result[test][build] = BASELINE_SUFFIX_LIST
        return result

    def _builds_to_tests(self, issue_number):
        """Fetches a list of try bots, and for each, fetches tests with new baselines."""
        _log.debug('Getting results for Rietveld issue %d.', issue_number)
        try_jobs = latest_try_jobs(issue_number, self._try_bots(), self.web)
        if not try_jobs:
            _log.debug('No try job results for builders in: %r.', self._try_bots())
        builds_to_tests = {}
        for job in try_jobs:
            test_results = self._unexpected_mismatch_results(job)
            build = Build(job.builder_name, job.build_number)
            builds_to_tests[build] = sorted(r.test_name() for r in test_results)
        return builds_to_tests

    def _try_bots(self):
        """Returns a collection of try bot builders to fetch results for."""
        return self._tool.builders.all_try_builder_names()

    def _unexpected_mismatch_results(self, try_job):
        """Fetches a list of LayoutTestResult objects for unexpected results with new baselines."""
        buildbot = self._tool.buildbot
        results_url = buildbot.results_url(try_job.builder_name, try_job.build_number)
        layout_test_results = buildbot.fetch_layout_test_results(results_url)
        if layout_test_results is None:
            _log.warning('Failed to request layout test results from "%s".', results_url)
            return []
        return layout_test_results.unexpected_mismatch_results()

    @staticmethod
    def _log_test_prefix_list(test_prefix_list):
        """Logs the tests to download new baselines for."""
        if not test_prefix_list:
            _log.info('No tests to rebaseline.')
            return
        _log.info('Tests to rebaseline:')
        for test, builds in test_prefix_list.iteritems():
            builds_str = ', '.join(sorted('%s (%s)' % (b.builder_name, b.build_number) for b in builds))
            _log.info('  %s: %s', test, builds_str)

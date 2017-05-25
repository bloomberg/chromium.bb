# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for the current CL."""

import json
import logging
import optparse

from webkitpy.common.net.git_cl import GitCL
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from webkitpy.tool.commands.rebaseline import TestBaselineSet
from webkitpy.w3c.wpt_manifest import WPTManifest

_log = logging.getLogger(__name__)

class RebaselineCL(AbstractParallelRebaselineCommand):
    name = 'rebaseline-cl'
    help_text = 'Fetches new baselines for a CL from test runs on try bots.'
    long_help = ('By default, this command will check the latest try job results '
                 'for all platforms, and start try jobs for platforms with no '
                 'try jobs. Then, new baselines are downloaded for any tests '
                 'that are being rebaselined. After downloading, the baselines '
                 'for different platforms will be optimized (consolidated).')
    show_in_main_help = True

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            optparse.make_option(
                '--dry-run', action='store_true', default=False,
                help='Dry run mode; list actions that would be performed but do not do anything.'),
            optparse.make_option(
                '--only-changed-tests', action='store_true', default=False,
                help='Only download new baselines for tests that are changed in the CL.'),
            optparse.make_option(
                '--no-trigger-jobs', dest='trigger_jobs', action='store_false', default=True,
                help='Do not trigger any try jobs.'),
            optparse.make_option(
                '--fill-missing', dest='fill_missing', action='store_true', default=False,
                help='If some platforms have no try job results, use results from try job results of other platforms.'),
            self.no_optimize_option,
            self.results_directory_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool
        # TODO(qyearsley): Consider calling ensure_manifest in Command or WebKitPatch.
        WPTManifest.ensure_manifest(tool)
        if not self.check_ok_to_run():
            return 1

        jobs = self.latest_try_jobs()
        self._log_scheduled_jobs(jobs)
        builders_with_no_jobs = self.builders_with_no_jobs(jobs)

        if options.trigger_jobs and builders_with_no_jobs:
            self.trigger_try_jobs(builders_with_no_jobs)
            return 1

        if not options.fill_missing and builders_with_no_jobs:
            _log.error('The following builders have no jobs:')
            for builder in builders_with_no_jobs:
                _log.error('  %s', builder)
            _log.error('Add --fill-missing to continue rebaselining anyway, '
                       'filling in results for missing platforms.')
            return 1

        jobs_to_results = self._fetch_results(jobs)
        if not options.fill_missing and len(jobs_to_results) < len(jobs):
            return 1

        if args:
            test_baseline_set = self._make_test_baseline_set_for_tests(
                args, jobs_to_results)
        else:
            test_baseline_set = self._make_test_baseline_set(
                jobs_to_results, options.only_changed_tests)

        if options.fill_missing:
            self.fill_in_missing_results(test_baseline_set)

        _log.debug('Rebaselining: %s', test_baseline_set)

        if not options.dry_run:
            self.rebaseline(options, test_baseline_set)
        return 0

    def check_ok_to_run(self):
        unstaged_baselines = self.unstaged_baselines()
        if unstaged_baselines:
            _log.error('Aborting: there are unstaged baselines:')
            for path in unstaged_baselines:
                _log.error('  %s', path)
            return False
        issue_number = self._get_issue_number()
        if issue_number is None:
            _log.error('No issue number for current branch.')
            return False
        return True

    def _get_issue_number(self):
        """Returns the current CL issue number, or None."""
        issue = self.git_cl().get_issue_number()
        if not issue.isdigit():
            return None
        return int(issue)

    def git_cl(self):
        """Returns a GitCL instance. Can be overridden for tests."""
        return GitCL(self._tool)

    def latest_try_jobs(self):
        """Lists the latest try jobs for the relevant try bots.

        This is a list of Build objects for jobs that have been triggered;
        for try jobs that are scheduled but not started, build_number is None.
        TODO(qyearsley): Also get and use information about the state of the
        job (started, success, failure).
        """
        return self.git_cl().latest_try_jobs(self._try_bots())

    def trigger_try_jobs(self, builders):
        """Triggers try jobs for the given builders."""
        _log.info('Triggering try jobs for:')
        for builder in sorted(builders):
            _log.info('  %s', builder)
        self.git_cl().trigger_try_jobs(builders)
        _log.info('Once all pending try jobs have finished, please re-run\n'
                  'webkit-patch rebaseline-cl to fetch new baselines.')

    def builders_with_no_jobs(self, builds):
        """Returns the set of builders that don't have triggered builds."""
        return set(self._try_bots()) - {b.builder_name for b in builds}

    def _log_scheduled_jobs(self, builds):
        builders = self._builders_with_scheduled_jobs(builds)
        if not builders:
            return
        _log.info('There are scheduled (but not yet started) builds for:')
        for builder in sorted(builders):
            _log.info('  %s', builder)

    def _builders_with_scheduled_jobs(self, builds):
        """Returns the set of builders that have scheduled builds
        that have not yet started."""
        return {b.builder_name for b in builds if b.build_number is None}

    def _try_bots(self):
        """Returns a collection of try bot builders to fetch results for."""
        return self._tool.builders.all_try_builder_names()

    def _fetch_results(self, builds):
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.

        Args:
            builds: A list of Build objects.

        Returns:
            A dict mapping Build to LayoutTestResults, or None if any results
            were not available.
        """
        buildbot = self._tool.buildbot
        results = {}
        for build in builds:
            results_url = buildbot.results_url(build.builder_name, build.build_number)
            layout_test_results = buildbot.fetch_results(build)
            if layout_test_results is None:
                _log.info('Failed to fetch results for %s', build)
                _log.info('Results URL: %s/results.html', results_url)
                continue
            results[build] = layout_test_results
        return results

    def _make_test_baseline_set_for_tests(self, tests, builds_to_results):
        test_baseline_set = TestBaselineSet(self._tool)
        for test in tests:
            for build in builds_to_results:
                test_baseline_set.add(test, build)
        return test_baseline_set

    def _make_test_baseline_set(self, builds_to_results, only_changed_tests):
        """Returns a dict which lists the set of baselines to fetch.

        The dict that is returned is a dict of tests to Build objects
        to baseline file extensions.

        Args:
            builds_to_results: A dict mapping Builds to LayoutTestResults.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A dict containing information about which new baselines to download.
        """
        builds_to_tests = {}
        for build, results in builds_to_results.iteritems():
            builds_to_tests[build] = self._tests_to_rebaseline(build, results)
        if only_changed_tests:
            files_in_cl = self._tool.git().changed_files(diff_filter='AM')
            # In the changed files list from Git, paths always use "/" as
            # the path separator, and they're always relative to repo root.
            # TODO(qyearsley): Do this without using a hard-coded constant.
            test_base = 'third_party/WebKit/LayoutTests/'
            tests_in_cl = [f[len(test_base):] for f in files_in_cl if f.startswith(test_base)]

        test_baseline_set = TestBaselineSet(self._tool)
        for build, tests in builds_to_tests.iteritems():
            for test in tests:
                if only_changed_tests and test not in tests_in_cl:
                    continue
                test_baseline_set.add(test, build)
        return test_baseline_set

    def _tests_to_rebaseline(self, build, layout_test_results):
        """Fetches a list of tests that should be rebaselined for some build."""
        unexpected_results = layout_test_results.didnt_run_as_expected_results()
        tests = sorted(r.test_name() for r in unexpected_results
                       if r.is_missing_baseline() or r.has_mismatch_result())

        new_failures = self._fetch_tests_with_new_failures(build)
        if new_failures is None:
            _log.warning('No retry summary available for build %s.', build)
        else:
            tests = [t for t in tests if t in new_failures]
        return tests

    def _fetch_tests_with_new_failures(self, build):
        """For a given try job, lists tests that only failed with the patch.

        If a test failed only with the patch but not without, then that
        indicates that the failure is actually related to the patch and
        is not failing at HEAD.

        If the list of new failures could not be obtained, this returns None.
        """
        buildbot = self._tool.buildbot
        content = buildbot.fetch_retry_summary_json(build)
        if content is None:
            return None
        try:
            retry_summary = json.loads(content)
            return retry_summary['failures']
        except (ValueError, KeyError):
            _log.warning('Unexpected retry summary content:\n%s', content)
            return None

    def fill_in_missing_results(self, test_baseline_set):
        """Adds entries, filling in results for missing jobs.

        For each test prefix, if there is an entry missing for some port,
        then an entry should be added for that port using a build that is
        available.

        For example, if there's no entry for the port "win-win7", but there
        is an entry for the "win-win10" port, then an entry might be added
        for "win-win7" using the results from "win-win10".
        """
        all_ports = {self._tool.builders.port_name_for_builder_name(b) for b in self._try_bots()}
        for test_prefix in test_baseline_set.test_prefixes():
            build_port_pairs = test_baseline_set.build_port_pairs(test_prefix)
            missing_ports = all_ports - {p for _, p in build_port_pairs}
            if not missing_ports:
                continue
            _log.info('For %s:', test_prefix)
            for port in missing_ports:
                build = self._choose_fill_in_build(port, build_port_pairs)
                _log.info('Using %s to supply results for %s.', build, port)
                test_baseline_set.add(test_prefix, build, port)
        return test_baseline_set

    def _choose_fill_in_build(self, target_port, build_port_pairs):
        """Returns a Build to use to supply results for the given port.

        Ideally, this should return a build for a similar port so that the
        results from the selected build may also be correct for the target port.
        """
        # A full port name should normally always be of the form <os>-<version>;
        # for example "win-win7", or "linux-trusty". For the test port used in
        # unit tests, though, the full port name may be "test-<os>-<version>".
        def os_name(port):
            if '-' not in port:
                return port
            return port[:port.rfind('-')]

        # If any Build exists with the same OS, use the first one.
        target_os = os_name(target_port)
        same_os_builds = sorted(b for b, p in build_port_pairs if os_name(p) == target_os)
        if same_os_builds:
            return same_os_builds[0]

        # Otherwise, perhaps any build will do, for example if the results are
        # the same on all platforms. In this case, just return the first build.
        return sorted(build_port_pairs)[0][0]

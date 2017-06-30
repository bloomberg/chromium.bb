# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for the current CL."""

import json
import logging
import optparse

from webkitpy.common.net.git_cl import GitCL, TryJobStatus
from webkitpy.common.path_finder import PathFinder
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from webkitpy.tool.commands.rebaseline import TestBaselineSet
from webkitpy.w3c.wpt_manifest import WPTManifest

_log = logging.getLogger(__name__)


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = 'rebaseline-cl'
    help_text = 'Fetches new baselines for a CL from test runs on try bots.'
    long_help = ('This command downloads new baselines for failing layout '
                 'tests from archived try job test results. Cross-platform '
                 'baselines are deduplicated after downloading.')
    show_in_main_help = True

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            optparse.make_option(
                '--dry-run', action='store_true', default=False,
                help='Dry run mode; list actions that would be performed but '
                     'do not actually download any new baselines.'),
            optparse.make_option(
                '--only-changed-tests', action='store_true', default=False,
                help='Only download new baselines for tests that are directly '
                     'modified in the CL.'),
            optparse.make_option(
                '--no-trigger-jobs', dest='trigger_jobs', action='store_false',
                default=True,
                help='Do not trigger any try jobs.'),
            optparse.make_option(
                '--fill-missing', dest='fill_missing', action='store_true',
                default=False,
                help='If some platforms have no try job results, use results '
                     'from try job results of other platforms.'),
            self.no_optimize_option,
            self.results_directory_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool

        # TODO(qyearsley): Consider calling ensure_manifest in WebKitPatch.
        # See: crbug.com/698294
        WPTManifest.ensure_manifest(tool)

        if not self.check_ok_to_run():
            return 1

        jobs = self.git_cl().latest_try_jobs(self._try_bots())
        self._log_jobs(jobs)
        builders_with_no_jobs = self._try_bots() - {b.builder_name for b in jobs}

        if not options.trigger_jobs and not jobs:
            _log.info('Aborted: no try jobs and --no-trigger-jobs passed.')
            return 1

        if options.trigger_jobs and builders_with_no_jobs:
            self.trigger_try_jobs(builders_with_no_jobs)
            return 1

        jobs_to_results = self._fetch_results(jobs)

        builders_with_results = {b.builder_name for b in jobs_to_results}
        builders_without_results = set(self._try_bots()) - builders_with_results
        if builders_without_results:
            _log.info('There are some builders with no results:')
            self._log_builder_list(builders_without_results)

        if not options.fill_missing and builders_without_results:
            options.fill_missing = self._tool.user.confirm(
                'Would you like to try to fill in missing results with\n'
                'available results? This assumes that layout test results\n'
                'for the platforms with missing results are the same as\n'
                'results on other platforms.',
                default=self._tool.user.DEFAULT_NO)
            if not options.fill_missing:
                _log.info('Aborting.')
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
        if self._get_issue_number() is None:
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

    def trigger_try_jobs(self, builders):
        """Triggers try jobs for the given builders."""
        _log.info('Triggering try jobs:')
        for builder in sorted(builders):
            _log.info('  %s', builder)
        self.git_cl().trigger_try_jobs(builders)
        _log.info('Once all pending try jobs have finished, please re-run\n'
                  'webkit-patch rebaseline-cl to fetch new baselines.')

    def _log_jobs(self, jobs):
        """Logs the current state of the try jobs.

        This includes which jobs were started or finished or missing,
        and their current state.

        Args:
            jobs: A dict mapping Build objects to TryJobStatus objects.
        """
        finished_jobs = {b for b, s in jobs.items() if s.status == 'COMPLETED'}
        if len(finished_jobs) == len(self._try_bots()):
            _log.info('Finished try jobs found for all try bots.')
            return

        if finished_jobs:
            _log.info('Finished try jobs:')
            self._log_builder_list({b.builder_name for b in finished_jobs})
        else:
            _log.info('No finished try jobs.')

        unfinished_jobs = {b for b in jobs if b not in finished_jobs}
        if unfinished_jobs:
            _log.info('Scheduled or started try jobs:')
            self._log_builder_list({b.builder_name for b in unfinished_jobs})

    def _log_builder_list(self, builders):
        for builder in sorted(builders):
            _log.info('  %s', builder)

    def _try_bots(self):
        """Returns a set of builder names to fetch results from."""
        return set(self._tool.builders.all_try_builder_names())

    def _fetch_results(self, jobs):
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.

        Args:
            jobs: A dict mapping Build objects to TryJobStatus objects.

        Returns:
            A dict mapping Build to LayoutTestResults for all completed jobs.
        """
        buildbot = self._tool.buildbot
        results = {}
        for build, status in jobs.iteritems():
            if status == TryJobStatus('COMPLETED', 'SUCCESS'):
                # Builds with passing try jobs are mapped to None, to indicate
                # that there are no baselines to download.
                results[build] = None
                continue
            if status != TryJobStatus('COMPLETED', 'FAILURE'):
                # Only completed failed builds will contain actual failed
                # layout tests to download baselines for.
                continue
            results_url = buildbot.results_url(build.builder_name, build.build_number)
            layout_test_results = buildbot.fetch_results(build)
            if layout_test_results is None:
                _log.info('Failed to fetch results for "%s".', build.builder_name)
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
            test_base = self._test_base_path()
            tests_in_cl = [f[len(test_base):] for f in files_in_cl if f.startswith(test_base)]

        test_baseline_set = TestBaselineSet(self._tool)
        for build, tests in builds_to_tests.iteritems():
            for test in tests:
                if only_changed_tests and test not in tests_in_cl:
                    continue
                test_baseline_set.add(test, build)
        return test_baseline_set

    def _test_base_path(self):
        """Returns the relative path from the repo root to the layout tests."""
        finder = PathFinder(self._tool.filesystem)
        return self._tool.filesystem.relpath(
            finder.layout_tests_dir(),
            finder.path_from_chromium_base()) + '/'

    def _tests_to_rebaseline(self, build, layout_test_results):
        """Fetches a list of tests that should be rebaselined for some build.

        Args:
            build: A Build instance.
            layout_test_results: A LayoutTestResults instance or None.

        Returns:
            A sorted list of tests to rebaseline for this build.
        """
        if layout_test_results is None:
            return []

        unexpected_results = layout_test_results.didnt_run_as_expected_results()
        tests = sorted(
            r.test_name() for r in unexpected_results
            if r.is_missing_baseline() or r.has_mismatch_result())

        new_failures = self._fetch_tests_with_new_failures(build)
        if new_failures is None:
            _log.warning('No retry summary available for "%s".', build.builder_name)
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
                _log.info(
                    'Using "%s" build %d for %s.',
                    build.builder_name, build.build_number, port)
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

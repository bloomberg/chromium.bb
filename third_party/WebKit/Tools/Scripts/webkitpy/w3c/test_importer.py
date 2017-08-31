# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches a copy of the latest state of a W3C test repository and commits.

If this script is given the argument --auto-update, it will also:
 1. Upload a CL.
 2. Trigger try jobs and wait for them to complete.
 3. Make any changes that are required for new failing tests.
 4. Attempt to land the CL.
"""

import argparse
import logging

from webkitpy.common.net.buildbot import current_build_link
from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.path_finder import PathFinder
from webkitpy.common.system.log_utils import configure_logging
from webkitpy.layout_tests.models.test_expectations import TestExpectations, TestExpectationParser
from webkitpy.layout_tests.port.base import Port
from webkitpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from webkitpy.w3c.common import read_credentials, is_testharness_baseline, is_file_exportable
from webkitpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.test_copier import TestCopier
from webkitpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from webkitpy.w3c.wpt_github import WPTGitHub
from webkitpy.w3c.wpt_manifest import WPTManifest

# Settings for how often to check try job results and how long to wait.
POLL_DELAY_SECONDS = 2 * 60
TIMEOUT_SECONDS = 180 * 60

_log = logging.getLogger(__file__)


class TestImporter(object):

    def __init__(self, host, wpt_github=None):
        self.host = host
        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = PathFinder(self.fs)
        self.verbose = False
        self.git_cl = None
        self.wpt_github = wpt_github
        self.dest_path = self.finder.path_from_layout_tests('external', 'wpt')

    def main(self, argv=None):
        options = self.parse_args(argv)

        self.verbose = options.verbose
        log_level = logging.DEBUG if self.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        if not self.checkout_is_okay():
            return 1

        credentials = read_credentials(self.host, options.credentials_json)
        gh_user = credentials.get('GH_USER')
        gh_token = credentials.get('GH_TOKEN')
        self.wpt_github = self.wpt_github or WPTGitHub(self.host, gh_user, gh_token)
        local_wpt = LocalWPT(self.host, gh_token=gh_token)
        self.git_cl = GitCL(self.host, auth_refresh_token_json=options.auth_refresh_token_json)

        _log.debug('Noting the current Chromium commit.')
        # TODO(qyearsley): Use Git (self.host.git) to run git commands.
        _, show_ref_output = self.run(['git', 'show-ref', '--verify', '--head', '--hash', 'HEAD'])
        chromium_commit = show_ref_output.strip()

        local_wpt.fetch()

        if options.revision is not None:
            _log.info('Checking out %s', options.revision)
            self.run(['git', 'checkout', options.revision], cwd=local_wpt.path)

        _log.debug('Noting the revision we are importing.')
        _, show_ref_output = self.run(['git', 'show-ref', 'origin/master'], cwd=local_wpt.path)
        import_commit = 'wpt@%s' % show_ref_output.split()[0]

        _log.info('Importing %s to Chromium %s', import_commit, chromium_commit)

        commit_message = self._commit_message(chromium_commit, import_commit)

        if not options.ignore_exportable_commits:
            commits = self.apply_exportable_commits_locally(local_wpt)
            if commits is None:
                _log.error('Could not apply some exportable commits cleanly.')
                _log.error('Aborting import to prevent clobbering commits.')
                return 1
            commit_message = self._commit_message(
                chromium_commit, import_commit, locally_applied_commits=commits)

        self._clear_out_dest_path()

        _log.info('Copying the tests from the temp repo to the destination.')
        test_copier = TestCopier(self.host, local_wpt.path)
        test_copier.do_import()

        self.run(['git', 'add', '--all', 'external/wpt'])

        self._generate_manifest()

        self._delete_orphaned_baselines()

        # TODO(qyearsley): Consider running the imported tests with
        # `run-webkit-tests --reset-results external/wpt` to get some baselines
        # before the try jobs are started.

        _log.info('Updating TestExpectations for any removed or renamed tests.')
        self.update_all_test_expectations_files(self._list_deleted_tests(), self._list_renamed_tests())

        has_changes = self._has_changes()
        if not has_changes:
            _log.info('Done: no changes to import.')
            return 0

        self._commit_changes(commit_message)
        _log.info('Changes imported and committed.')

        if not options.auto_update:
            return 0

        self._upload_cl()
        _log.info('Issue: %s', self.git_cl.run(['issue']).strip())

        if not self.update_expectations_for_cl():
            return 1

        if not self.run_commit_queue_for_cl():
            return 1

        return 0

    def update_expectations_for_cl(self):
        """Performs the expectation-updating part of an auto-import job.

        This includes triggering try jobs and waiting; then, if applicable,
        writing new baselines and TestExpectation lines, committing, and
        uploading a new patchset.

        This assumes that there is CL associated with the current branch.

        Returns True if everything is OK to continue, or False on failure.
        """
        _log.info('Triggering try jobs for updating expectations.')
        self.git_cl.trigger_try_jobs(self.blink_try_bots())
        try_results = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS)

        if not try_results:
            _log.error('No initial try job results, aborting.')
            self.git_cl.run(['set-close'])
            return False

        if try_results and self.git_cl.some_failed(try_results):
            self.fetch_new_expectations_and_baselines()
            if self.host.git().has_working_directory_changes():
                self._generate_manifest()
                message = 'Update test expectations and baselines.'
                self.check_run(['git', 'commit', '-a', '-m', message])
                self._upload_patchset(message)
        return True

    def run_commit_queue_for_cl(self):
        """Triggers CQ and either commits or aborts; returns True on success."""
        _log.info('Triggering CQ try jobs.')
        self.git_cl.run(['try'])
        try_results = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS)
        try_results = self.git_cl.filter_latest(try_results)

        # We only want to check the status of CQ bots. The set of CQ bots is
        # determined by //infra/config/cq.cfg, but since in import jobs we only
        # trigger CQ bots and Blink try bots, we just ignore the
        # Blink try bots to get the set of CQ try bots.
        # Important: if any CQ bots are added to the builder list
        # (self.host.builders), then this logic will need to be updated.
        try_results = {build: status for build, status in try_results.items()
                       if build.builder_name not in self.blink_try_bots()}

        if not try_results:
            self.git_cl.run(['set-close'])
            _log.error('No CQ try job results, aborting.')
            return False

        if try_results and self.git_cl.all_success(try_results):
            _log.info('CQ appears to have passed; trying to commit.')
            self.git_cl.run(['upload', '-f', '--send-mail'])  # Turn off WIP mode.
            self.git_cl.run(['set-commit'])
            _log.info('Update completed.')
            return True

        self.git_cl.run(['set-close'])
        _log.error('CQ appears to have failed; aborting.')
        return False

    def blink_try_bots(self):
        """Returns the collection of builders used for updating expectations."""
        return self.host.builders.all_try_builder_names()

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument(
            '-v', '--verbose', action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--ignore-exportable-commits', action='store_true',
            help='do not check for exportable commits that would be clobbered')
        parser.add_argument('-r', '--revision', help='target wpt revision')
        parser.add_argument(
            '--auto-update', action='store_true',
            help='upload a CL, update expectations, and trigger CQ')
        parser.add_argument(
            '--auth-refresh-token-json',
            help='authentication refresh token JSON file used for try jobs, '
                 'generally not necessary on developer machines')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with GitHub credentials, '
                 'generally not necessary on developer machines')

        return parser.parse_args(argv)

    def checkout_is_okay(self):
        git_diff_retcode, _ = self.run(
            ['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        if git_diff_retcode:
            _log.warning('Checkout is dirty; aborting.')
            return False
        _, local_commits = self.run(
            ['git', 'log', '--oneline', 'origin/master..HEAD'])
        if local_commits:
            _log.warning('Checkout has local commits before import.')
        return True

    def apply_exportable_commits_locally(self, local_wpt):
        """Applies exportable Chromium changes to the local WPT repo.

        The purpose of this is to avoid clobbering changes that were made in
        Chromium but not yet merged upstream. By applying these changes to the
        local copy of web-platform-tests before copying files over, we make
        it so that the resulting change in Chromium doesn't undo the
        previous Chromium change.

        Args:
            A LocalWPT instance for our local copy of WPT.

        Returns:
            A list of commits applied (could be empty), or None if any
            of the patches could not be applied cleanly.
        """
        commits = self.exportable_but_not_exported_commits(local_wpt)
        for commit in commits:
            _log.info('Applying exportable commit locally:')
            _log.info(commit.url())
            _log.info('Subject: %s', commit.subject().strip())
            # TODO(qyearsley): We probably don't need to know about
            # corresponding PRs at all anymore, although this information
            # could still be useful for reference.
            pull_request = self.wpt_github.pr_for_chromium_commit(commit)
            if pull_request:
                _log.info('PR: https://github.com/w3c/web-platform-tests/pull/%d', pull_request.number)
            else:
                _log.warning('No pull request found.')
            error = local_wpt.apply_patch(commit.format_patch())
            if error:
                _log.error('Commit cannot be applied cleanly:')
                _log.error(error)
                return None
            self.run(
                ['git', 'commit', '--all', '-F', '-'],
                stdin='Applying patch %s' % commit.sha,
                cwd=local_wpt.path)
        return commits

    def exportable_but_not_exported_commits(self, local_wpt):
        """Returns a list of commits that would be clobbered by importer.

        The list contains all exportable but not exported commits, not filtered
        by whether they can apply cleanly.
        """
        # The errors returned by exportable_commits_over_last_n_commits are
        # irrelevant and ignored here, because it tests patches *individually*
        # while the importer tries to reapply these patches *cumulatively*.
        commits, _ = exportable_commits_over_last_n_commits(
            self.host, local_wpt, self.wpt_github, require_clean=False)
        return commits

    def _generate_manifest(self):
        """Generates MANIFEST.json for imported tests.

        Runs the (newly-updated) manifest command if it's found, and then
        stages the generated MANIFEST.json in the git index, ready to commit.
        """
        _log.info('Generating MANIFEST.json')
        WPTManifest.generate_manifest(self.host, self.dest_path)
        manifest_path = self.fs.join(self.dest_path, 'MANIFEST.json')
        assert self.fs.exists(manifest_path)
        manifest_base_path = self.fs.normpath(
            self.fs.join(self.dest_path, '..', 'WPT_BASE_MANIFEST.json'))
        self.copyfile(manifest_path, manifest_base_path)
        self.run(['git', 'add', manifest_base_path])

    def _clear_out_dest_path(self):
        """Removes all files that are synced with upstream from Chromium WPT.

        Instead of relying on TestCopier to overwrite these files, cleaning up
        first ensures if upstream deletes some files, we also delete them.
        """
        _log.info('Cleaning out tests from %s.', self.dest_path)
        should_remove = lambda fs, dirname, basename: (
            is_file_exportable(fs.relpath(fs.join(dirname, basename), self.finder.chromium_base())))
        files_to_delete = self.fs.files_under(self.dest_path, file_filter=should_remove)
        for subpath in files_to_delete:
            self.remove(self.finder.path_from_layout_tests('external', subpath))

    def _commit_changes(self, commit_message):
        _log.info('Committing changes.')
        self.run(['git', 'commit', '--all', '-F', '-'], stdin=commit_message)

    def _has_changes(self):
        return_code, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        return return_code == 1

    def _commit_message(self, chromium_commit_sha, import_commit_sha,
                        locally_applied_commits=None):
        message = 'Import {}\n\nUsing wpt-import in Chromium {}.\n'.format(
            import_commit_sha, chromium_commit_sha)
        if locally_applied_commits:
            message += 'With Chromium commits locally applied on WPT:\n'
            message += '\n'.join(str(commit) for commit in locally_applied_commits)
        message += '\nNo-Export: true'
        return message

    def _delete_orphaned_baselines(self):
        _log.info('Deleting any orphaned baselines.')

        is_baseline_filter = lambda fs, dirname, basename: is_testharness_baseline(basename)

        baselines = self.fs.files_under(self.dest_path, file_filter=is_baseline_filter)

        # TODO(qyearsley): Factor out the manifest path to a common location.
        # TODO(qyearsley): Factor out the manifest reading from here and Port
        # to WPTManifest.
        manifest_path = self.finder.path_from_layout_tests('external', 'wpt', 'MANIFEST.json')
        manifest = WPTManifest(self.fs.read_text_file(manifest_path))
        wpt_urls = manifest.all_urls()

        # Currently baselines for tests with query strings are merged,
        # so that the tests foo.html?r=1 and foo.html?r=2 both have the same
        # baseline, foo-expected.txt.
        # TODO(qyearsley): Remove this when this behavior is fixed.
        wpt_urls = [url.split('?')[0] for url in wpt_urls]

        wpt_dir = self.finder.path_from_layout_tests('external', 'wpt')
        for full_path in baselines:
            rel_path = self.fs.relpath(full_path, wpt_dir)
            if not self._has_corresponding_test(rel_path, wpt_urls):
                self.fs.remove(full_path)

    def _has_corresponding_test(self, rel_path, wpt_urls):
        # TODO(qyearsley): Ensure that this works with platform baselines and
        # virtual baselines, and add unit tests.
        base = '/' + rel_path.replace('-expected.txt', '')
        return any((base + ext) in wpt_urls for ext in Port.supported_file_extensions)

    def run(self, cmd, exit_on_failure=True, cwd=None, stdin=''):
        _log.debug('Running command: %s', ' '.join(cmd))

        cwd = cwd or self.finder.path_from_layout_tests()
        proc = self.executive.popen(cmd, stdout=self.executive.PIPE, stderr=self.executive.PIPE, stdin=self.executive.PIPE, cwd=cwd)
        out, err = proc.communicate(stdin)
        if proc.returncode or self.verbose:
            _log.info('# ret> %d', proc.returncode)
            if out:
                for line in out.splitlines():
                    _log.info('# out> %s', line)
            if err:
                for line in err.splitlines():
                    _log.info('# err> %s', line)
        if exit_on_failure and proc.returncode:
            self.host.exit(proc.returncode)
        return proc.returncode, out

    def check_run(self, command):
        return_code, out = self.run(command)
        if return_code:
            raise Exception('%s failed with exit code %d.' % ' '.join(command), return_code)
        return out

    def copyfile(self, source, destination):
        _log.debug('cp %s %s', source, destination)
        self.fs.copyfile(source, destination)

    def remove(self, dest):
        _log.debug('rm %s', dest)
        self.fs.remove(dest)

    def _upload_patchset(self, message):
        self.git_cl.run(['upload', '-f', '-t', message, '--gerrit'])

    def _upload_cl(self):
        _log.info('Uploading change list.')
        directory_owners = self.get_directory_owners()
        description = self._cl_description(directory_owners)
        self.git_cl.run([
            'upload',
            '-f',
            '--gerrit',
            '-m',
            description,
            '--tbrs',
            'qyearsley@chromium.org',
        ] + self._cc_part(directory_owners))

    @staticmethod
    def _cc_part(directory_owners):
        cc_part = []
        for owner_tuple in sorted(directory_owners):
            cc_part.extend('--cc=' + owner for owner in owner_tuple)
        return cc_part

    def get_directory_owners(self):
        """Returns a mapping of email addresses to owners of changed tests."""
        _log.info('Gathering directory owners emails to CC.')
        changed_files = self.host.git().changed_files()
        extractor = DirectoryOwnersExtractor(self.fs)
        return extractor.list_owners(changed_files)

    def _cl_description(self, directory_owners):
        """Returns a CL description string.

        Args:
            directory_owners: A dict of tuples of owner names to lists of directories.
        """
        description = self.check_run(['git', 'log', '-1', '--format=%B'])
        build_link = current_build_link(self.host)
        if build_link:
            description += 'Build: %s\n\n' % build_link

        description += (
            'Note to sheriffs: This CL imports external tests and adds\n'
            'expectations for those tests; if this CL is large and causes\n'
            'a few new failures, please fix the failures by adding new\n'
            'lines to TestExpectations rather than reverting. See:\n'
            'https://chromium.googlesource.com'
            '/chromium/src/+/master/docs/testing/web_platform_tests.md\n\n')

        if directory_owners:
            description += self._format_directory_owners(directory_owners) + '\n\n'

        # Move any No-Export tag to the end of the description.
        description = description.replace('No-Export: true', '')
        description = description.replace('\n\n\n\n', '\n\n')
        description += 'No-Export: true'
        return description

    @staticmethod
    def _format_directory_owners(directory_owners):
        message_lines = ['Directory owners for changes in this CL:']
        for owner_tuple, directories in sorted(directory_owners.items()):
            message_lines.append(', '.join(owner_tuple) + ':')
            message_lines.extend('  ' + d for d in directories)
        return '\n'.join(message_lines)

    def fetch_new_expectations_and_baselines(self):
        """Modifies expectation lines and baselines based on try job results.

        Assuming that there are some try job results available, this
        adds new expectation lines to TestExpectations and downloads new
        baselines based on the try job results.

        This is the same as invoking the `wpt-update-expectations` script.
        """
        _log.info('Adding test expectations lines to LayoutTests/TestExpectations.')
        expectation_updater = WPTExpectationsUpdater(self.host)
        expectation_updater.run(args=[])

    def update_all_test_expectations_files(self, deleted_tests, renamed_tests):
        """Updates all test expectations files for tests that have been deleted or renamed.

        This is only for deleted or renamed tests in the initial import,
        not for tests that have failures in try jobs.
        """
        port = self.host.port_factory.get()
        for path, file_contents in port.all_expectations_dict().iteritems():
            parser = TestExpectationParser(port, all_tests=None, is_lint_mode=False)
            expectation_lines = parser.parse(path, file_contents)
            self._update_single_test_expectations_file(path, expectation_lines, deleted_tests, renamed_tests)

    def _update_single_test_expectations_file(self, path, expectation_lines, deleted_tests, renamed_tests):
        """Updates a single test expectations file."""
        # FIXME: This won't work for removed or renamed directories with test
        # expectations that are directories rather than individual tests.
        new_lines = []
        changed_lines = []
        for expectation_line in expectation_lines:
            if expectation_line.name in deleted_tests:
                continue
            if expectation_line.name in renamed_tests:
                expectation_line.name = renamed_tests[expectation_line.name]
                # Upon parsing the file, a "path does not exist" warning is expected
                # to be there for tests that have been renamed, and if there are warnings,
                # then the original string is used. If the warnings are reset, then the
                # expectation line is re-serialized when output.
                expectation_line.warnings = []
                changed_lines.append(expectation_line)
            new_lines.append(expectation_line)
        new_file_contents = TestExpectations.list_to_string(new_lines, reconstitute_only_these=changed_lines)
        self.host.filesystem.write_text_file(path, new_file_contents)

    def _list_deleted_tests(self):
        """List of layout tests that have been deleted."""
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=D', '--name-only'])
        deleted_tests = []
        for path in out.splitlines():
            test = self._relative_to_layout_test_dir(path)
            if test:
                deleted_tests.append(test)
        return deleted_tests

    def _list_renamed_tests(self):
        """Lists tests that have been renamed.

        Returns a dict mapping source name to destination name.
        """
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=R', '--name-status'])
        renamed_tests = {}
        for line in out.splitlines():
            _, source_path, dest_path = line.split()
            source_test = self._relative_to_layout_test_dir(source_path)
            dest_test = self._relative_to_layout_test_dir(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests

    def _relative_to_layout_test_dir(self, path_relative_to_repo_root):
        """Returns a path that's relative to the layout tests directory."""
        abs_path = self.finder.path_from_chromium_base(path_relative_to_repo_root)
        if not abs_path.startswith(self.finder.layout_tests_dir()):
            return None
        return self.fs.relpath(abs_path, self.finder.layout_tests_dir())

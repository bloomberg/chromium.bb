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

from webkitpy.common.net.git_cl import GitCL, TryJobStatus
from webkitpy.common.net.buildbot import current_build_link
from webkitpy.common.path_finder import PathFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectations, TestExpectationParser
from webkitpy.layout_tests.port.base import Port
from webkitpy.w3c.common import WPT_REPO_URL, WPT_DEST_NAME, read_credentials, exportable_commits_over_last_n_commits
from webkitpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.wpt_github import WPTGitHub
from webkitpy.w3c.test_copier import TestCopier
from webkitpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
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

    def main(self, argv=None):
        options = self.parse_args(argv)

        credentials = read_credentials(self.host, options.credentials_json)
        if self.wpt_github is None:
            self.wpt_github = WPTGitHub(
                self.host, credentials.get('GH_USER'), credentials.get('GH_TOKEN'))

        self.verbose = options.verbose
        log_level = logging.DEBUG if self.verbose else logging.INFO
        logging.basicConfig(level=log_level, format='%(message)s')

        if not self.checkout_is_okay(options.allow_local_commits):
            return 1

        self.git_cl = GitCL(self.host, auth_refresh_token_json=options.auth_refresh_token_json)

        _log.debug('Noting the current Chromium commit.')
        _, show_ref_output = self.run(['git', 'show-ref', 'HEAD'])
        chromium_commit = show_ref_output.split()[0]

        dest_dir_name = WPT_DEST_NAME
        repo_url = WPT_REPO_URL

        # TODO(qyearsley): Simplify this to use LocalWPT.fetch when csswg-test
        # is merged into web-platform-tests (crbug.com/706118).
        temp_repo_path = self.finder.path_from_layout_tests(dest_dir_name)
        _log.info('Cloning repo: %s', repo_url)
        _log.info('Local path: %s', temp_repo_path)
        self.run(['git', 'clone', repo_url, temp_repo_path])

        if not options.ignore_exportable_commits:
            commits = self.exportable_but_not_exported_commits(temp_repo_path)
            if commits:
                _log.info('There were exportable but not-yet-exported commits:')
                for commit in commits:
                    _log.info('Commit: %s', commit.url())
                    _log.info('Subject: %s', commit.subject().strip())
                    pull_request = self.wpt_github.pr_with_position(commit.position)
                    if pull_request:
                        _log.info('PR: https://github.com/w3c/web-platform-tests/pull/%d', pull_request.number)
                    else:
                        _log.warning('No pull request found.')
                    _log.info('Modified files in wpt directory in this commit:')
                    for path in commit.filtered_changed_files():
                        _log.info('  %s', path)
                _log.info('Aborting import to prevent clobbering commits.')
                self.clean_up_temp_repo(temp_repo_path)
                return 0

        import_commit = self.update(dest_dir_name, temp_repo_path, options.revision)

        self.clean_up_temp_repo(temp_repo_path)

        has_changes = self._has_changes()
        if not has_changes:
            _log.info('Done: no changes to import.')
            return 0

        commit_message = self._commit_message(chromium_commit, import_commit)
        self._commit_changes(commit_message)
        _log.info('Done: changes imported and committed.')

        if options.auto_update:
            commit_successful = self.do_auto_update()
            if not commit_successful:
                return 1
        return 0

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument('-v', '--verbose', action='store_true',
                            help='log what we are doing')
        parser.add_argument('--allow-local-commits', action='store_true',
                            help='allow script to run even if we have local commits')
        parser.add_argument('-r', dest='revision', action='store',
                            help='Target revision.')
        parser.add_argument('--auto-update', action='store_true',
                            help='uploads CL and initiates commit queue.')
        parser.add_argument('--auth-refresh-token-json',
                            help='authentication refresh token JSON file, '
                                 'used for authentication for try jobs, '
                                 'generally not necessary on developer machines')
        parser.add_argument('--credentials-json',
                            help='A JSON file with an object containing zero or more of the '
                                 'following keys: GH_USER, GH_TOKEN')
        parser.add_argument('--ignore-exportable-commits', action='store_true',
                            help='Continue even if there are exportable commits that may be overwritten.')

        return parser.parse_args(argv)

    def checkout_is_okay(self, allow_local_commits):
        git_diff_retcode, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        if git_diff_retcode:
            _log.warning('Checkout is dirty; aborting.')
            return False

        local_commits = self.run(['git', 'log', '--oneline', 'origin/master..HEAD'])[1]
        if local_commits and not allow_local_commits:
            _log.warning('Checkout has local commits; aborting. Use --allow-local-commits to allow this.')
            return False

        temp_repo_path = self.finder.path_from_layout_tests(WPT_DEST_NAME)
        if self.fs.exists(temp_repo_path):
            _log.warning('%s exists; aborting.', temp_repo_path)
            return False

        return True

    def exportable_but_not_exported_commits(self, wpt_path):
        """Checks for commits that might be overwritten by importing and lists them.

        Args:
            wpt_path: The path to a local checkout of web-platform-tests.

        Returns:
            A list of commits in the Chromium repo that are exportable
            but not yet exported to the web-platform-tests repo.
        """
        assert self.host.filesystem.exists(wpt_path)
        local_wpt = LocalWPT(self.host, path=wpt_path)
        return exportable_commits_over_last_n_commits(
            self.host, local_wpt, self.wpt_github)

    def clean_up_temp_repo(self, temp_repo_path):
        """Removes the temporary copy of the wpt repo that was downloaded."""
        _log.info('Deleting temp repo directory %s.', temp_repo_path)
        self.fs.rmtree(temp_repo_path)

    def _generate_manifest(self, dest_path):
        """Generates MANIFEST.json for imported tests.

        Args:
            dest_path: Path to the destination WPT directory.

        Runs the (newly-updated) manifest command if it's found, and then
        stages the generated MANIFEST.json in the git index, ready to commit.
        """
        _log.info('Generating MANIFEST.json')
        WPTManifest.generate_manifest(self.host, dest_path)
        manifest_path = self.fs.join(dest_path, 'MANIFEST.json')
        assert self.fs.exists(manifest_path)
        manifest_base_path = self.fs.normpath(
            self.fs.join(dest_path, '..', 'WPT_BASE_MANIFEST.json'))
        self.copyfile(manifest_path, manifest_base_path)
        self.run(['git', 'add', manifest_base_path])

    def update(self, dest_dir_name, temp_repo_path, revision):
        """Updates an imported repository.

        Args:
            dest_dir_name: The destination directory name.
            temp_repo_path: Path to local checkout of W3C test repo.
            revision: A W3C test repo commit hash, or None.

        Returns:
            A string for the commit description "<destination>@<commitish>".
        """
        if revision is not None:
            _log.info('Checking out %s', revision)
            self.run(['git', 'checkout', revision], cwd=temp_repo_path)

        self.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=temp_repo_path)

        _log.info('Noting the revision we are importing.')
        _, show_ref_output = self.run(['git', 'show-ref', 'origin/master'], cwd=temp_repo_path)
        master_commitish = show_ref_output.split()[0]

        dest_path = self.finder.path_from_layout_tests('external', dest_dir_name)
        self._clear_out_dest_path(dest_path)

        _log.info('Importing the tests.')
        test_copier = TestCopier(self.host, temp_repo_path)
        test_copier.do_import()

        self.run(['git', 'add', '--all', 'external/%s' % dest_dir_name])

        self._delete_orphaned_baselines(dest_path)

        self._generate_manifest(dest_path)

        # TODO(qyearsley): Consider running the imported tests with
        # `run-webkit-tests --reset-results external/wpt` to get most of
        # the cross-platform baselines without having to wait for try jobs.
        # TODO(qyearsley): Consider updating manifest after adding baselines.
        # TODO(qyearsley): Consider starting CQ at the same time as the
        # initial try jobs.

        _log.info('Updating TestExpectations for any removed or renamed tests.')
        self.update_all_test_expectations_files(self._list_deleted_tests(), self._list_renamed_tests())

        return '%s@%s' % (dest_dir_name, master_commitish)

    def _clear_out_dest_path(self, dest_path):
        _log.info('Cleaning out tests from %s.', dest_path)
        should_remove = lambda fs, dirname, basename: (
            not self.is_baseline(basename) and
            # See http://crbug.com/702283 for context.
            basename != 'OWNERS')
        files_to_delete = self.fs.files_under(dest_path, file_filter=should_remove)
        for subpath in files_to_delete:
            self.remove(self.finder.path_from_layout_tests('external', subpath))

    def _commit_changes(self, commit_message):
        _log.info('Committing changes.')
        self.run(['git', 'commit', '--all', '-F', '-'], stdin=commit_message)

    def _has_changes(self):
        return_code, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        return return_code == 1

    def _commit_message(self, chromium_commit, import_commit):
        return ('Import %s\n\n'
                'Using wpt-import in Chromium %s.\n\n'
                'NOEXPORT=true' %
                (import_commit, chromium_commit))

    def _delete_orphaned_baselines(self, dest_path):
        _log.info('Deleting any orphaned baselines.')
        is_baseline_filter = lambda fs, dirname, basename: self.is_baseline(basename)
        previous_baselines = self.fs.files_under(dest_path, file_filter=is_baseline_filter)
        for sub_path in previous_baselines:
            full_baseline_path = self.fs.join(dest_path, sub_path)
            if not self._has_corresponding_test(full_baseline_path):
                self.fs.remove(full_baseline_path)

    def _has_corresponding_test(self, full_baseline_path):
        base = full_baseline_path.replace('-expected.txt', '')
        return any(self.fs.exists(base + ext) for ext in Port.supported_file_extensions)

    @staticmethod
    def is_baseline(basename):
        # TODO(qyearsley): Find a better, centralized place for this.
        # Also, the name for this method should be is_text_baseline.
        return basename.endswith('-expected.txt')

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

    def do_auto_update(self):
        """Attempts to upload a CL, make any required adjustments, and commit.

        This function assumes that the imported repo has already been updated,
        and that change has been committed. There may be newly-failing tests,
        so before being able to commit these new changes, we may need to update
        TestExpectations or download new baselines.

        Returns:
            True if successfully committed, False otherwise.
        """
        self._upload_cl()
        _log.info('Issue: %s', self.git_cl.run(['issue']).strip())

        # First, try on Blink try bots in order to get any new baselines.
        # TODO(qyearsley): Make this faster by triggering all try jobs in
        # one invocation.
        _log.info('Triggering try jobs.')
        self.git_cl.trigger_try_jobs()
        try_results = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS, timeout_seconds=TIMEOUT_SECONDS)

        if not try_results:
            _log.error('No initial try job results, aborting.')
            self.git_cl.run(['set-close'])
            return False

        if try_results and self.git_cl.has_failing_try_results(try_results):
            self.fetch_new_expectations_and_baselines()
            message = 'Update test expectations and baselines.'
            self.check_run(['git', 'commit', '-a', '-m', message])
            self._upload_patchset(message)

        # Trigger CQ and wait for CQ try jobs to finish.
        self.git_cl.run(['try'])
        try_results = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS)

        if not try_results:
            self.git_cl.run(['set-close'])
            _log.error('No CQ try job results, aborting.')
            return False

        if try_results and all(s == TryJobStatus('COMPLETED', 'SUCCESS') for _, s in try_results.iteritems()):
            _log.info('CQ appears to have passed; trying to commit.')
            self.git_cl.run(['upload', '-f', '--send-mail'])  # Turn off WIP mode.
            self.git_cl.run(['set-commit'])
            _log.info('Update completed.')
            return True

        self.git_cl.run(['set-close'])
        _log.error('CQ appears to have failed; aborting.')
        return False

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
        ] + self._cc_part(directory_owners))

    def _upload_patchset(self, message):
        self.git_cl.run(['upload', '-f', '-t', message, '--gerrit'])

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
        extractor.read_owner_map()
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
        description += 'TBR=qyearsley@chromium.org\n'

        # Move any NOEXPORT tag to the end of the description.
        description = description.replace('NOEXPORT=true', '')
        description = description.replace('\n\n\n\n', '\n\n')
        description += 'NOEXPORT=true'
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
        # FIXME: This won't work for removed or renamed directories with test expectations
        # that are directories rather than individual tests.
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
        for line in out.splitlines():
            test = self.finder.layout_test_name(line)
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
            source_test = self.finder.layout_test_name(source_path)
            dest_test = self.finder.layout_test_name(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests

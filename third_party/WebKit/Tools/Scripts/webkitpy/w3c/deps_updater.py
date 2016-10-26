# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches a copy of the latest state of a W3C test repository and commits.

If this script is given the argument --auto-update, it will also attempt to
upload a CL, triggery try jobs, and make any changes that are required for
new failing tests before committing.
"""

import argparse
import json

from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectations, TestExpectationParser

# Import destination directories (under LayoutTests/imported/).
WPT_DEST_NAME = 'wpt'
CSS_DEST_NAME = 'csswg-test'

# Our mirrors of the official w3c repos, which we pull from.
WPT_REPO_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
CSS_REPO_URL = 'https://chromium.googlesource.com/external/w3c/csswg-test.git'

POLL_DELAY_SECONDS = 900


class DepsUpdater(object):

    def __init__(self, host):
        self.host = host
        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = WebKitFinder(self.fs)
        self.verbose = False
        self.git_cl = None

    def main(self, argv=None):
        options = self.parse_args(argv)
        self.verbose = options.verbose

        if not self.checkout_is_okay(options.allow_local_commits):
            return 1

        self.git_cl = GitCL(self.host, auth_refresh_token_json=options.auth_refresh_token_json)

        self.print_('## Noting the current Chromium commit.')
        _, show_ref_output = self.run(['git', 'show-ref', 'HEAD'])
        chromium_commitish = show_ref_output.split()[0]

        if options.target == 'wpt':
            import_commitish = self.update(WPT_DEST_NAME, WPT_REPO_URL, options.keep_w3c_repos_around, options.revision)
            self._copy_resources()
        elif options.target == 'css':
            import_commitish = self.update(CSS_DEST_NAME, CSS_REPO_URL, options.keep_w3c_repos_around, options.revision)
        else:
            raise AssertionError("Unsupported target %s" % options.target)

        has_changes = self.commit_changes_if_needed(chromium_commitish, import_commitish)
        if options.auto_update and has_changes:
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
        parser.add_argument('--keep-w3c-repos-around', action='store_true',
                            help='leave the w3c repos around that were imported previously.')
        parser.add_argument('-r', dest='revision', action='store',
                            help='Target revision.')
        parser.add_argument('target', choices=['css', 'wpt'],
                            help='Target repository.  "css" for csswg-test, "wpt" for web-platform-tests.')
        parser.add_argument('--auto-update', action='store_true',
                            help='uploads CL and initiates commit queue.')
        parser.add_argument('--auth-refresh-token-json',
                            help='Rietveld auth refresh JSON token.')
        return parser.parse_args(argv)

    def checkout_is_okay(self, allow_local_commits):
        git_diff_retcode, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        if git_diff_retcode:
            self.print_('## Checkout is dirty; aborting.')
            return False

        local_commits = self.run(['git', 'log', '--oneline', 'origin/master..HEAD'])[1]
        if local_commits and not allow_local_commits:
            self.print_('## Checkout has local commits; aborting. Use --allow-local-commits to allow this.')
            return False

        if self.fs.exists(self.path_from_webkit_base(WPT_DEST_NAME)):
            self.print_('## WebKit/%s exists; aborting.' % WPT_DEST_NAME)
            return False

        if self.fs.exists(self.path_from_webkit_base(CSS_DEST_NAME)):
            self.print_('## WebKit/%s repo exists; aborting.' % CSS_DEST_NAME)
            return False

        return True

    def _copy_resources(self):
        """Copies resources from LayoutTests/resources to wpt and vice versa.

        There are resources from our repository that we use instead of the
        upstream versions. Conversely, there are also some resources that
        are copied in the other direction.

        Specifically:
          - testharnessreport.js contains code needed to integrate our testing
            with testharness.js; we also want our code to be used for tests
            in wpt.
          - TODO(qyearsley, jsbell): Document why other other files are copied,
            or stop copying them if it's unnecessary.

        If this method is changed, the lists of files expected to be identical
        in LayoutTests/PRESUBMIT.py should also be changed.
        """
        resources_to_copy_to_wpt = [
            ('testharnessreport.js', 'resources'),
            ('WebIDLParser.js', 'resources'),
            ('vendor-prefix.js', 'common'),
        ]
        resources_to_copy_from_wpt = [
            ('idlharness.js', 'resources'),
            ('testharness.js', 'resources'),
        ]
        for filename, wpt_subdir in resources_to_copy_to_wpt:
            source = self.path_from_webkit_base('LayoutTests', 'resources', filename)
            destination = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, wpt_subdir, filename)
            self.copyfile(source, destination)
            self.run(['git', 'add', destination])
        for filename, wpt_subdir in resources_to_copy_from_wpt:
            source = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, wpt_subdir, filename)
            destination = self.path_from_webkit_base('LayoutTests', 'resources', filename)
            self.copyfile(source, destination)
            self.run(['git', 'add', destination])

    def update(self, dest_dir_name, url, keep_w3c_repos_around, revision):
        """Updates an imported repository.

        Args:
            dest_dir_name: The destination directory name.
            url: URL of the git repository.
            revision: Commit hash or None.

        Returns:
            A string for the commit description "<destination>@<commitish>".
        """
        temp_repo_path = self.path_from_webkit_base(dest_dir_name)
        self.print_('## Cloning %s into %s.' % (url, temp_repo_path))
        self.run(['git', 'clone', url, temp_repo_path])

        if revision is not None:
            self.print_('## Checking out %s' % revision)
            self.run(['git', 'checkout', revision], cwd=temp_repo_path)
        self.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=temp_repo_path)

        self.print_('## Noting the revision we are importing.')
        _, show_ref_output = self.run(['git', 'show-ref', 'origin/master'], cwd=temp_repo_path)
        master_commitish = show_ref_output.split()[0]

        self.print_('## Cleaning out tests from LayoutTests/imported/%s.' % dest_dir_name)
        dest_path = self.path_from_webkit_base('LayoutTests', 'imported', dest_dir_name)
        files_to_delete = self.fs.files_under(dest_path, file_filter=self.is_not_baseline)
        for subpath in files_to_delete:
            self.remove('LayoutTests', 'imported', subpath)

        self.print_('## Importing the tests.')
        src_repo = self.path_from_webkit_base(dest_dir_name)
        import_path = self.path_from_webkit_base('Tools', 'Scripts', 'import-w3c-tests')
        self.run([self.host.executable, import_path, '-d', 'imported', src_repo])

        self.run(['git', 'add', '--all', 'LayoutTests/imported/%s' % dest_dir_name])

        self.print_('## Deleting manual tests.')
        files_to_delete = self.fs.files_under(dest_path, file_filter=self.is_manual_test)
        for subpath in files_to_delete:
            self.remove('LayoutTests', 'imported', subpath)

        self.print_('## Deleting any orphaned baselines.')
        previous_baselines = self.fs.files_under(dest_path, file_filter=self.is_baseline)
        for subpath in previous_baselines:
            full_path = self.fs.join(dest_path, subpath)
            if self.fs.glob(full_path.replace('-expected.txt', '*')) == [full_path]:
                self.fs.remove(full_path)

        if not keep_w3c_repos_around:
            self.print_('## Deleting temp repo directory %s.' % temp_repo_path)
            self.rmtree(temp_repo_path)

        self.print_('## Updating TestExpectations for any removed or renamed tests.')
        self.update_all_test_expectations_files(self._list_deleted_tests(), self._list_renamed_tests())

        return '%s@%s' % (dest_dir_name, master_commitish)

    def commit_changes_if_needed(self, chromium_commitish, import_commitish):
        if self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)[0]:
            self.print_('## Committing changes.')
            commit_msg = ('Import %s\n'
                          '\n'
                          'Using update-w3c-deps in Chromium %s.\n'
                          % (import_commitish, chromium_commitish))
            path_to_commit_msg = self.path_from_webkit_base('commit_msg')
            if self.verbose:
                self.print_('cat > %s <<EOF' % path_to_commit_msg)
                self.print_(commit_msg)
                self.print_('EOF')
            self.fs.write_text_file(path_to_commit_msg, commit_msg)
            self.run(['git', 'commit', '-a', '-F', path_to_commit_msg])
            self.remove(path_to_commit_msg)
            self.print_('## Done: changes imported and committed.')
            return True
        else:
            self.print_('## Done: no changes to import.')
            return False

    def is_manual_test(self, fs, dirname, basename):
        """Returns True if the file should be removed because it's a manual test.

        Tests with "-manual" in the name are not considered manual tests
        if there is a corresponding JS automation file.
        """
        basename_without_extension, _ = self.fs.splitext(basename)
        if not basename_without_extension.endswith('-manual'):
            return False
        dir_from_wpt = fs.relpath(dirname, self.path_from_webkit_base('LayoutTests', 'imported', 'wpt'))
        automation_dir = self.path_from_webkit_base('LayoutTests', 'imported', 'wpt_automation', dir_from_wpt)
        if fs.isfile(fs.join(automation_dir, '%s-automation.js' % basename_without_extension)):
            return False
        return True

    # Callback for FileSystem.files_under; not all arguments used - pylint: disable=unused-argument
    def is_baseline(self, fs, dirname, basename):
        return basename.endswith('-expected.txt')

    def is_not_baseline(self, fs, dirname, basename):
        return not self.is_baseline(fs, dirname, basename)

    def run(self, cmd, exit_on_failure=True, cwd=None):
        if self.verbose:
            self.print_(' '.join(cmd))

        cwd = cwd or self.finder.webkit_base()
        proc = self.executive.popen(cmd, stdout=self.executive.PIPE, stderr=self.executive.PIPE, cwd=cwd)
        out, err = proc.communicate()
        if proc.returncode or self.verbose:
            self.print_('# ret> %d' % proc.returncode)
            if out:
                for line in out.splitlines():
                    self.print_('# out> %s' % line)
            if err:
                for line in err.splitlines():
                    self.print_('# err> %s' % line)
        if exit_on_failure and proc.returncode:
            self.host.exit(proc.returncode)
        return proc.returncode, out

    def check_run(self, command):
        return_code, out = self.run(command)
        if return_code:
            raise Exception('%s failed with exit code %d.' % ' '.join(command), return_code)
        return out

    def copyfile(self, source, destination):
        if self.verbose:
            self.print_('cp %s %s' % (source, destination))
        self.fs.copyfile(source, destination)

    def remove(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        if self.verbose:
            self.print_('rm %s' % dest)
        self.fs.remove(dest)

    def rmtree(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        if self.verbose:
            self.print_('rm -fr %s' % dest)
        self.fs.rmtree(dest)

    def path_from_webkit_base(self, *comps):
        return self.finder.path_from_webkit_base(*comps)

    def print_(self, msg):
        self.host.print_(msg)

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
        self.print_('## ' + self.git_cl.run(['issue']).strip())

        # First try: if there are failures, update expectations.
        self.print_('## Triggering try jobs.')
        for try_bot in self.host.builders.all_try_builder_names():
            self.git_cl.run(['try', '-b', try_bot])
        try_results = self.git_cl.wait_for_try_jobs()
        if not try_results:
            self.print_('## Timed out waiting for try results.')
            return
        if try_results and self.git_cl.has_failing_try_results(try_results):
            self.fetch_new_expectations_and_baselines()

        # Second try: if there are failures, then abort.
        self.git_cl.run(['set-commit', '--rietveld'])
        try_results = self.git_cl.wait_for_try_jobs()
        if not try_results:
            self.print_('Timed out waiting for try results.')
            self.git_cl.run(['set-close'])
            return False
        if self.git_cl.has_failing_try_results(try_results):
            self.print_('CQ failed; aborting.')
            self.git_cl.run(['set-close'])
            return False
        self.print_('## Update completed.')
        return True

    def _upload_cl(self):
        self.print_('## Uploading change list.')
        cc_list = self.get_directory_owners_to_cc()
        last_commit_message = self.check_run(['git', 'log', '-1', '--format=%B'])
        commit_message = last_commit_message + 'TBR=qyearsley@chromium.org'
        self.git_cl.run([
            'upload',
            '-f',
            '--rietveld',
            '-m',
            commit_message,
        ] + ['--cc=' + email for email in cc_list])

    def get_directory_owners_to_cc(self):
        """Returns a list of email addresses to CC for the current import."""
        self.print_('## Gathering directory owners emails to CC.')
        directory_owners_file_path = self.finder.path_from_webkit_base(
            'Tools', 'Scripts', 'webkitpy', 'w3c', 'directory_owners.json')
        with open(directory_owners_file_path) as data_file:
            directory_to_owner = self.parse_directory_owners(json.load(data_file))
        out = self.check_run(['git', 'diff', 'origin/master', '--name-only'])
        changed_files = out.splitlines()
        return self.generate_email_list(changed_files, directory_to_owner)

    @staticmethod
    def parse_directory_owners(decoded_data_file):
        directory_dict = {}
        for dict_set in decoded_data_file:
            if dict_set['notification-email']:
                directory_dict[dict_set['directory']] = dict_set['notification-email']
        return directory_dict

    def generate_email_list(self, changed_files, directory_to_owner):
        """Returns a list of email addresses based on the given file list and
        directory-to-owner mapping.

        Args:
            changed_files: A list of file paths relative to the repository root.
            directory_to_owner: A dict mapping layout test directories to emails.

        Returns:
            A list of the email addresses to be notified for the current import.
        """
        email_addresses = set()
        for file_path in changed_files:
            test_path = self.finder.layout_test_name(file_path)
            if test_path is None:
                continue
            test_dir = self.fs.dirname(test_path)
            if test_dir in directory_to_owner:
                email_addresses.add(directory_to_owner[test_dir])
        return sorted(email_addresses)

    def fetch_new_expectations_and_baselines(self):
        """Adds new expectations and downloads baselines based on try job results, then commits and uploads the change."""
        self.print_('## Adding test expectations lines to LayoutTests/TestExpectations.')
        script_path = self.path_from_webkit_base('Tools', 'Scripts', 'update-w3c-test-expectations')
        self.run([self.host.executable, script_path, '--verbose'])
        message = 'Modify TestExpectations or download new baselines for tests.'
        self.check_run(['git', 'commit', '-a', '-m', message])
        self.git_cl.run(['upload', '-m', message, '--rietveld'])

    def update_all_test_expectations_files(self, deleted_tests, renamed_tests):
        """Updates all test expectations files for tests that have been deleted or renamed."""
        port = self.host.port_factory.get()
        for path, file_contents in port.all_expectations_dict().iteritems():

            parser = TestExpectationParser(port, all_tests=None, is_lint_mode=False)
            expectation_lines = parser.parse(path, file_contents)
            self._update_single_test_expectations_file(path, expectation_lines, deleted_tests, renamed_tests)

    def _update_single_test_expectations_file(self, path, expectation_lines, deleted_tests, renamed_tests):
        """Updates single test expectations file."""
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
        """Returns a list of layout tests that have been deleted."""
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=D', '--name-only'])
        deleted_tests = []
        for line in out.splitlines():
            test = self.finder.layout_test_name(line)
            if test:
                deleted_tests.append(test)
        return deleted_tests

    def _list_renamed_tests(self):
        """Returns a dict mapping source to dest name for layout tests that have been renamed."""
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=R', '--name-status'])
        renamed_tests = {}
        for line in out.splitlines():
            _, source_path, dest_path = line.split()
            source_test = self.finder.layout_test_name(source_path)
            dest_test = self.finder.layout_test_name(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests

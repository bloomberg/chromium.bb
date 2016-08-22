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

        self.git_cl = GitCL(self.executive, auth_refresh_token_json=options.auth_refresh_token_json)

        self.print_('## Noting the current Chromium commit.')
        _, show_ref_output = self.run(['git', 'show-ref', 'HEAD'])
        chromium_commitish = show_ref_output.split()[0]

        if options.target == 'wpt':
            import_commitish = self.update(WPT_DEST_NAME, WPT_REPO_URL, options.keep_w3c_repos_around)
            self._copy_resources_to_wpt()
        elif options.target == 'css':
            import_commitish = self.update(CSS_DEST_NAME, CSS_REPO_URL, options.keep_w3c_repos_around)
        else:
            raise AssertionError("Unsupported target %s" % options.target)

        has_changes = self.commit_changes_if_needed(chromium_commitish, import_commitish)
        if options.auto_update and has_changes:
            commit_successful = self.do_auto_update(options.auth_refresh_token_json)
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

    def _copy_resources_to_wpt(self):
        """Copies some files over to a newly-updated wpt directory.

        There are some resources in our repository that we use instead of the
        upstream versions.
        """
        resources_to_copy = [
            ('testharnessreport.js', 'resources'),
            ('WebIDLParser.js', 'resources'),
            ('vendor-prefix.js', 'common'),
        ]
        for filename, wpt_subdir in resources_to_copy:
            source = self.path_from_webkit_base('LayoutTests', 'resources', filename)
            destination = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, wpt_subdir, filename)
            self.copyfile(source, destination)
            self.run(['git', 'add', destination])

    def update(self, dest_dir_name, url, keep_w3c_repos_around):
        """Updates an imported repository.

        Args:
            dest_dir_name: The destination directory name.
            url: URL of the git repository.

        Returns:
            A string for the commit description "<destination>@<commitish>".
        """
        temp_repo_path = self.path_from_webkit_base(dest_dir_name)
        self.print_('## Cloning %s into %s.' % (url, temp_repo_path))
        self.run(['git', 'clone', url, temp_repo_path])

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

    def is_baseline(self, fs, dirname, basename):  # Callback for FileSystem.files_under; not all arguments used - pylint: disable=unused-argument
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

    def do_auto_update(self, auth_refresh_token_json):
        """Attempts to upload a CL, make any required adjustments, and commit.

        This function assumes that the imported repo has already been updated,
        and that change has been committed. There may be newly-failing tests,
        so before being able to commit these new changes, we may need to update
        TestExpectations or download new baselines.

        Returns:
            True if successfully committed, False otherwise.
        """
        self.print_('## Uploading change list.')
        cc_list = self.get_directory_owners_to_cc()
        self.git_cl.run([
            'upload', '-f', '--rietveld'
            '-m', 'W3C auto test import CL.\n\nTBR=qyearsley@chromium.org',
        ] + ['--cc=' + email for email in cc_list])

        self.print_('## Triggering try jobs.')
        for try_bot in self.host.builders.all_try_builder_names():
            self.git_cl.run(['try', '-b', try_bot])
        if self.git_cl.has_failing_try_results():
            self.write_test_expectations()

        self.run(['set-commit', '--rietveld'])
        if self.git_cl.has_failing_try_results():
            self.print_('## CL has failing results when trying to land; aborting.')
            self.git_cl.run(['set-close'])
            return False
        return True

    def get_directory_owners_to_cc(self):
        """Returns a list of email addresses to CC for the current import."""
        self.print_('## Gathering directory owners emails to CC.')
        directory_owners_file_path = self.finder.path_from_webkit_base(
            'Tools', 'Scripts', 'webkitpy', 'w3c', 'directory_owners.json')
        with open(directory_owners_file_path) as data_file:
            directory_to_owner = self.parse_directory_owners(json.load(data_file))
        out = self.check_run(['git', 'diff', 'master', '--name-only'])
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
            A list of the email addresses to be notified for the current
            import.
        """
        email_addresses = set()
        for file_path in changed_files:
            test_path = self.finder.layout_test_name(file_path)
            test_dir = self.fs.dirname(test_path)
            if test_dir in directory_to_owner:
                email_addresses.add(directory_to_owner[test_dir])
        return sorted(email_addresses)

    def write_test_expectations(self):
        self.print_('## Adding test expectations lines to LayoutTests/TestExpectations.')
        script_path = self.path_from_webkit_base('Tools', 'Scripts', 'update-w3c-test-expectations')
        self.run([self.host.executable, script_path])
        message = '\'Modifies TestExpectations and/or downloads new baselines for tests\''
        self.check_run(['git', 'commit', '-a', '-m', message])
        self.git_cl(['upload', '-m', message, '--rietveld'])

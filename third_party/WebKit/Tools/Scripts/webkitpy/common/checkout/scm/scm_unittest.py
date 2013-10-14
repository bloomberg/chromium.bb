# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2011 Daniel Bates (dbates@intudata.com). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import atexit
import base64
import codecs
import getpass
import os
import os.path
import re
import stat
import sys
import subprocess
import tempfile
import time
import webkitpy.thirdparty.unittest2 as unittest
import urllib
import shutil

from datetime import date
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.executive_mock import MockExecutive
from .git import Git, AmbiguousCommitError
from .detection import detect_scm_system
from .scm import SCM
from .svn import SVN


# We cache the mock SVN repo so that we don't create it again for each call to an SVNTest or GitTest test_ method.
# We store it in a global variable so that we can delete this cached repo on exit(3).
# FIXME: Remove this once we migrate to Python 2.7. Unittest in Python 2.7 supports module-specific setup and teardown functions.
cached_svn_repo_path = None


def remove_dir(path):
    # Change directory to / to ensure that we aren't in the directory we want to delete.
    os.chdir('/')
    shutil.rmtree(path)


# FIXME: Remove this once we migrate to Python 2.7. Unittest in Python 2.7 supports module-specific setup and teardown functions.
@atexit.register
def delete_cached_mock_repo_at_exit():
    if cached_svn_repo_path:
        remove_dir(cached_svn_repo_path)

# Eventually we will want to write tests which work for both scms. (like update_webkit, changed_files, etc.)
# Perhaps through some SCMTest base-class which both SVNTest and GitTest inherit from.

def run_command(*args, **kwargs):
    # FIXME: This should not be a global static.
    # New code should use Executive.run_command directly instead
    return Executive().run_command(*args, **kwargs)


# FIXME: This should be unified into one of the executive.py commands!
# Callers could use run_and_throw_if_fail(args, cwd=cwd, quiet=True)
def run_silent(args, cwd=None):
    # Note: Not thread safe: http://bugs.python.org/issue2320
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)
    process.communicate() # ignore output
    exit_code = process.wait()
    if exit_code:
        raise ScriptError('Failed to run "%s"  exit_code: %d  cwd: %s' % (args, exit_code, cwd))


def write_into_file_at_path(file_path, contents, encoding="utf-8"):
    if encoding:
        with codecs.open(file_path, "w", encoding) as file:
            file.write(contents)
    else:
        with open(file_path, "w") as file:
            file.write(contents)


def read_from_path(file_path, encoding="utf-8"):
    with codecs.open(file_path, "r", encoding) as file:
        return file.read()


def _make_diff(command, *args):
    # We use this wrapper to disable output decoding. diffs should be treated as
    # binary files since they may include text files of multiple differnet encodings.
    # FIXME: This should use an Executive.
    return run_command([command, "diff"] + list(args), decode_output=False)


def _svn_diff(*args):
    return _make_diff("svn", *args)


def _git_diff(*args):
    return _make_diff("git", *args)


# Exists to share svn repository creation code between the git and svn tests
class SVNTestRepository(object):
    @classmethod
    def _svn_add(cls, path):
        run_command(["svn", "add", path])

    @classmethod
    def _svn_commit(cls, message):
        run_command(["svn", "commit", "--quiet", "--message", message])

    @classmethod
    def _setup_test_commits(cls, svn_repo_url):

        svn_checkout_path = tempfile.mkdtemp(suffix="svn_test_checkout")
        run_command(['svn', 'checkout', '--quiet', svn_repo_url, svn_checkout_path])

        # Add some test commits
        os.chdir(svn_checkout_path)

        write_into_file_at_path("test_file", "test1")
        cls._svn_add("test_file")
        cls._svn_commit("initial commit")

        write_into_file_at_path("test_file", "test1test2")
        # This used to be the last commit, but doing so broke
        # GitTest.test_apply_git_patch which use the inverse diff of the last commit.
        # svn-apply fails to remove directories in Git, see:
        # https://bugs.webkit.org/show_bug.cgi?id=34871
        os.mkdir("test_dir")
        # Slash should always be the right path separator since we use cygwin on Windows.
        test_file3_path = "test_dir/test_file3"
        write_into_file_at_path(test_file3_path, "third file")
        cls._svn_add("test_dir")
        cls._svn_commit("second commit")

        write_into_file_at_path("test_file", "test1test2test3\n")
        write_into_file_at_path("test_file2", "second file")
        cls._svn_add("test_file2")
        cls._svn_commit("third commit")

        # This 4th commit is used to make sure that our patch file handling
        # code correctly treats patches as binary and does not attempt to
        # decode them assuming they're utf-8.
        write_into_file_at_path("test_file", u"latin1 test: \u00A0\n", "latin1")
        write_into_file_at_path("test_file2", u"utf-8 test: \u00A0\n", "utf-8")
        cls._svn_commit("fourth commit")

        # svn does not seem to update after commit as I would expect.
        run_command(['svn', 'update'])
        remove_dir(svn_checkout_path)

    # This is a hot function since it's invoked by unittest before calling each test_ method in SVNTest and
    # GitTest. We create a mock SVN repo once and then perform an SVN checkout from a filesystem copy of
    # it since it's expensive to create the mock repo.
    @classmethod
    def setup(cls, test_object):
        global cached_svn_repo_path
        if not cached_svn_repo_path:
            cached_svn_repo_path = cls._setup_mock_repo()

        test_object.temp_directory = tempfile.mkdtemp(suffix="svn_test")
        test_object.svn_repo_path = os.path.join(test_object.temp_directory, "repo")
        test_object.svn_repo_url = "file://%s" % test_object.svn_repo_path
        test_object.svn_checkout_path = os.path.join(test_object.temp_directory, "checkout")
        shutil.copytree(cached_svn_repo_path, test_object.svn_repo_path)
        run_command(['svn', 'checkout', '--quiet', test_object.svn_repo_url + "/trunk", test_object.svn_checkout_path])

    @classmethod
    def _setup_mock_repo(cls):
        # Create an test SVN repository
        svn_repo_path = tempfile.mkdtemp(suffix="svn_test_repo")
        svn_repo_url = "file://%s" % svn_repo_path  # Not sure this will work on windows
        # git svn complains if we don't pass --pre-1.5-compatible, not sure why:
        # Expected FS format '2'; found format '3' at /usr/local/libexec/git-core//git-svn line 1477
        run_command(['svnadmin', 'create', '--pre-1.5-compatible', svn_repo_path])

        # Create a test svn checkout
        svn_checkout_path = tempfile.mkdtemp(suffix="svn_test_checkout")
        run_command(['svn', 'checkout', '--quiet', svn_repo_url, svn_checkout_path])

        # Create and checkout a trunk dir to match the standard svn configuration to match git-svn's expectations
        os.chdir(svn_checkout_path)
        os.mkdir('trunk')
        cls._svn_add('trunk')
        # We can add tags and branches as well if we ever need to test those.
        cls._svn_commit('add trunk')

        # Change directory out of the svn checkout so we can delete the checkout directory.
        remove_dir(svn_checkout_path)

        cls._setup_test_commits(svn_repo_url + "/trunk")
        return svn_repo_path

    @classmethod
    def tear_down(cls, test_object):
        remove_dir(test_object.temp_directory)

        # Now that we've deleted the checkout paths, cwddir may be invalid
        # Change back to a valid directory so that later calls to os.getcwd() do not fail.
        if os.path.isabs(__file__):
            path = os.path.dirname(__file__)
        else:
            path = sys.path[0]
        os.chdir(detect_scm_system(path).checkout_root)


# GitTest and SVNTest inherit from this so any test_ methods here will be run once for this class and then once for each subclass.
class SCMTest(unittest.TestCase):
    # Tests which both GitTest and SVNTest should run.
    # FIXME: There must be a simpler way to add these w/o adding a wrapper method to both subclasses

    # FIXME: pylint gets confused by this being a mixin, doesn't realize that self.scm exists.
    # pylint: disable-msg=E1101

    def _shared_test_add_recursively(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())

    def _shared_test_delete_recursively(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())
        self.scm.delete("added_dir/added_file")
        self.assertNotIn("added_dir", self.scm._added_files())

    def _shared_test_delete_recursively_or_not(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        write_into_file_at_path("added_dir/another_added_file", "more new stuff")
        self.scm.add("added_dir/added_file")
        self.scm.add("added_dir/another_added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())
        self.assertIn("added_dir/another_added_file", self.scm._added_files())
        self.scm.delete("added_dir/added_file")
        self.assertIn("added_dir/another_added_file", self.scm._added_files())

    def _shared_test_exists(self, scm, commit_function):
        os.chdir(scm.checkout_root)
        self.assertFalse(scm.exists('foo.txt'))
        write_into_file_at_path('foo.txt', 'some stuff')
        self.assertFalse(scm.exists('foo.txt'))
        scm.add('foo.txt')
        commit_function('adding foo')
        self.assertTrue(scm.exists('foo.txt'))
        scm.delete('foo.txt')
        commit_function('deleting foo')
        self.assertFalse(scm.exists('foo.txt'))

    def _shared_test_move(self):
        write_into_file_at_path('added_file', 'new stuff')
        self.scm.add('added_file')
        self.scm.move('added_file', 'moved_file')
        self.assertIn('moved_file', self.scm._added_files())

    def _shared_test_move_recursive(self):
        os.mkdir("added_dir")
        write_into_file_at_path('added_dir/added_file', 'new stuff')
        write_into_file_at_path('added_dir/another_added_file', 'more new stuff')
        self.scm.add('added_dir')
        self.scm.move('added_dir', 'moved_dir')
        self.assertIn('moved_dir/added_file', self.scm._added_files())
        self.assertIn('moved_dir/another_added_file', self.scm._added_files())


# Context manager that overrides the current timezone.
class TimezoneOverride(object):
    def __init__(self, timezone_string):
        self._timezone_string = timezone_string

    def __enter__(self):
        if hasattr(time, 'tzset'):
            self._saved_timezone = os.environ.get('TZ', None)
            os.environ['TZ'] = self._timezone_string
            time.tzset()

    def __exit__(self, type, value, traceback):
        if hasattr(time, 'tzset'):
            if self._saved_timezone:
                os.environ['TZ'] = self._saved_timezone
            else:
                del os.environ['TZ']
            time.tzset()


class SVNTest(SCMTest):

    @staticmethod
    def _set_date_and_reviewer(changelog_entry):
        # Joe Cool matches the reviewer set in SCMTest._create_patch
        changelog_entry = changelog_entry.replace('REVIEWER_HERE', 'Joe Cool')
        # svn-apply will update ChangeLog entries with today's date (as in Cupertino, CA, US)
        with TimezoneOverride('PST8PDT'):
            return changelog_entry.replace('DATE_HERE', date.today().isoformat())

    def setUp(self):
        SVNTestRepository.setup(self)
        os.chdir(self.svn_checkout_path)
        self.scm = detect_scm_system(self.svn_checkout_path)
        self.scm.svn_server_realm = None

    def tearDown(self):
        SVNTestRepository.tear_down(self)

    def test_detect_scm_system_relative_url(self):
        scm = detect_scm_system(".")
        # I wanted to assert that we got the right path, but there was some
        # crazy magic with temp folder names that I couldn't figure out.
        self.assertTrue(scm.checkout_root)

    def test_detection(self):
        self.assertEqual(self.scm.display_name(), "svn")
        self.assertEqual(self.scm.supports_local_commits(), False)

    def test_add_recursively(self):
        self._shared_test_add_recursively()

    def test_delete(self):
        os.chdir(self.svn_checkout_path)
        self.scm.delete("test_file")
        self.assertIn("test_file", self.scm._deleted_files())

    def test_delete_list(self):
        os.chdir(self.svn_checkout_path)
        self.scm.delete_list(["test_file", "test_file2"])
        self.assertIn("test_file", self.scm._deleted_files())
        self.assertIn("test_file2", self.scm._deleted_files())

    def test_delete_recursively(self):
        self._shared_test_delete_recursively()

    def test_delete_recursively_or_not(self):
        self._shared_test_delete_recursively_or_not()

    def test_move(self):
        self._shared_test_move()

    def test_move_recursive(self):
        self._shared_test_move_recursive()


class GitTest(SCMTest):

    def setUp(self):
        """Sets up fresh git repository with one commit. Then setups a second git
        repo that tracks the first one."""
        # FIXME: We should instead clone a git repo that is tracking an SVN repo.
        # That better matches what we do with WebKit.
        self.original_dir = os.getcwd()

        self.untracking_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout2")
        run_command(['git', 'init', self.untracking_checkout_path])

        os.chdir(self.untracking_checkout_path)
        write_into_file_at_path('foo_file', 'foo')
        run_command(['git', 'add', 'foo_file'])
        run_command(['git', 'commit', '-am', 'dummy commit'])
        self.untracking_scm = detect_scm_system(self.untracking_checkout_path)

        self.tracking_git_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout")
        run_command(['git', 'clone', '--quiet', self.untracking_checkout_path, self.tracking_git_checkout_path])
        os.chdir(self.tracking_git_checkout_path)
        self.tracking_scm = detect_scm_system(self.tracking_git_checkout_path)

    def tearDown(self):
        # Change back to a valid directory so that later calls to os.getcwd() do not fail.
        os.chdir(self.original_dir)
        run_command(['rm', '-rf', self.tracking_git_checkout_path])
        run_command(['rm', '-rf', self.untracking_checkout_path])

    def test_remote_branch_ref(self):
        self.assertEqual(self.tracking_scm._remote_branch_ref(), 'refs/remotes/origin/master')

        os.chdir(self.untracking_checkout_path)
        self.assertRaises(ScriptError, self.untracking_scm._remote_branch_ref)

    def test_multiple_remotes(self):
        run_command(['git', 'config', '--add', 'svn-remote.svn.fetch', 'trunk:remote1'])
        run_command(['git', 'config', '--add', 'svn-remote.svn.fetch', 'trunk:remote2'])
        self.assertEqual(self.tracking_scm._remote_branch_ref(), 'remote1')

    def test_create_patch(self):
        write_into_file_at_path('test_file_commit1', 'contents')
        run_command(['git', 'add', 'test_file_commit1'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        patch = scm.create_patch()
        self.assertNotRegexpMatches(patch, r'Subversion Revision:')

    def test_exists(self):
        scm = self.untracking_scm
        self._shared_test_exists(scm, scm.commit_locally_with_message)

    def test_rename_files(self):
        scm = self.tracking_scm

        scm.move('foo_file', 'bar_file')
        scm.commit_locally_with_message('message')


class GitSVNTest(SCMTest):

    def _setup_git_checkout(self):
        self.git_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout")
        # --quiet doesn't make git svn silent, so we use run_silent to redirect output
        run_silent(['git', 'svn', 'clone', '-T', 'trunk', self.svn_repo_url, self.git_checkout_path])
        os.chdir(self.git_checkout_path)

    def _tear_down_git_checkout(self):
        # Change back to a valid directory so that later calls to os.getcwd() do not fail.
        os.chdir(self.original_dir)
        run_command(['rm', '-rf', self.git_checkout_path])

    def setUp(self):
        self.original_dir = os.getcwd()

        SVNTestRepository.setup(self)
        self._setup_git_checkout()
        self.scm = detect_scm_system(self.git_checkout_path)
        self.scm.svn_server_realm = None

    def tearDown(self):
        SVNTestRepository.tear_down(self)
        self._tear_down_git_checkout()

    def test_detection(self):
        self.assertEqual(self.scm.display_name(), "git")
        self.assertEqual(self.scm.supports_local_commits(), True)

    def test_read_git_config(self):
        key = 'test.git-config'
        value = 'git-config value'
        run_command(['git', 'config', key, value])
        self.assertEqual(self.scm.read_git_config(key), value)

    def test_local_commits(self):
        test_file = os.path.join(self.git_checkout_path, 'test_file')
        write_into_file_at_path(test_file, 'foo')
        run_command(['git', 'commit', '-a', '-m', 'local commit'])

        self.assertEqual(len(self.scm._local_commits()), 1)

    def test_discard_local_commits(self):
        test_file = os.path.join(self.git_checkout_path, 'test_file')
        write_into_file_at_path(test_file, 'foo')
        run_command(['git', 'commit', '-a', '-m', 'local commit'])

        self.assertEqual(len(self.scm._local_commits()), 1)
        self.scm._discard_local_commits()
        self.assertEqual(len(self.scm._local_commits()), 0)

    def test_delete_branch(self):
        new_branch = 'foo'

        run_command(['git', 'checkout', '-b', new_branch])
        self.assertEqual(run_command(['git', 'symbolic-ref', 'HEAD']).strip(), 'refs/heads/' + new_branch)

        run_command(['git', 'checkout', '-b', 'bar'])
        self.scm.delete_branch(new_branch)

        self.assertNotRegexpMatches(run_command(['git', 'branch']), r'foo')

    def test_rebase_in_progress(self):
        svn_test_file = os.path.join(self.svn_checkout_path, 'test_file')
        write_into_file_at_path(svn_test_file, "svn_checkout")
        run_command(['svn', 'commit', '--message', 'commit to conflict with git commit'], cwd=self.svn_checkout_path)

        git_test_file = os.path.join(self.git_checkout_path, 'test_file')
        write_into_file_at_path(git_test_file, "git_checkout")
        run_command(['git', 'commit', '-a', '-m', 'commit to be thrown away by rebase abort'])

        # --quiet doesn't make git svn silent, so use run_silent to redirect output
        self.assertRaises(ScriptError, run_silent, ['git', 'svn', '--quiet', 'rebase']) # Will fail due to a conflict leaving us mid-rebase.

        self.assertTrue(self.scm._rebase_in_progress())

        # Make sure our cleanup works.
        self.scm._discard_working_directory_changes()
        self.assertFalse(self.scm._rebase_in_progress())

        # Make sure cleanup doesn't throw when no rebase is in progress.
        self.scm._discard_working_directory_changes()

    def _local_commit(self, filename, contents, message):
        write_into_file_at_path(filename, contents)
        run_command(['git', 'add', filename])
        self.scm.commit_locally_with_message(message)

    def _one_local_commit(self):
        self._local_commit('test_file_commit1', 'more test content', 'another test commit')

    def _one_local_commit_plus_working_copy_changes(self):
        self._one_local_commit()
        write_into_file_at_path('test_file_commit2', 'still more test content')
        run_command(['git', 'add', 'test_file_commit2'])

    def _second_local_commit(self):
        self._local_commit('test_file_commit2', 'still more test content', 'yet another test commit')

    def _two_local_commits(self):
        self._one_local_commit()
        self._second_local_commit()

    def _three_local_commits(self):
        self._local_commit('test_file_commit0', 'more test content', 'another test commit')
        self._two_local_commits()

    def test_locally_commit_all_working_copy_changes(self):
        self._local_commit('test_file', 'test content', 'test commit')
        write_into_file_at_path('test_file', 'changed test content')
        self.assertTrue(self.scm.has_working_directory_changes())
        self.scm.commit_locally_with_message('all working copy changes')
        self.assertFalse(self.scm.has_working_directory_changes())

    def test_locally_commit_no_working_copy_changes(self):
        self._local_commit('test_file', 'test content', 'test commit')
        write_into_file_at_path('test_file', 'changed test content')
        self.assertTrue(self.scm.has_working_directory_changes())
        self.assertRaises(ScriptError, self.scm.commit_locally_with_message, 'no working copy changes', False)

    def _test_upstream_branch(self):
        run_command(['git', 'checkout', '-t', '-b', 'my-branch'])
        run_command(['git', 'checkout', '-t', '-b', 'my-second-branch'])
        self.assertEqual(self.scm._upstream_branch(), 'my-branch')

    def test_remote_branch_ref(self):
        self.assertEqual(self.scm._remote_branch_ref(), 'refs/remotes/trunk')

    def test_create_patch_local_plus_working_copy(self):
        self._one_local_commit_plus_working_copy_changes()
        patch = self.scm.create_patch()
        self.assertRegexpMatches(patch, r'test_file_commit1')
        self.assertRegexpMatches(patch, r'test_file_commit2')

    def test_create_patch(self):
        self._one_local_commit_plus_working_copy_changes()
        patch = self.scm.create_patch()
        self.assertRegexpMatches(patch, r'test_file_commit2')
        self.assertRegexpMatches(patch, r'test_file_commit1')
        self.assertRegexpMatches(patch, r'Subversion Revision: 5')

    def test_create_patch_after_merge(self):
        run_command(['git', 'checkout', '-b', 'dummy-branch', 'trunk~3'])
        self._one_local_commit()
        run_command(['git', 'merge', 'trunk'])

        patch = self.scm.create_patch()
        self.assertRegexpMatches(patch, r'test_file_commit1')
        self.assertRegexpMatches(patch, r'Subversion Revision: 5')

    def test_create_patch_with_changed_files(self):
        self._one_local_commit_plus_working_copy_changes()
        patch = self.scm.create_patch(changed_files=['test_file_commit2'])
        self.assertRegexpMatches(patch, r'test_file_commit2')

    def test_create_patch_with_rm_and_changed_files(self):
        self._one_local_commit_plus_working_copy_changes()
        os.remove('test_file_commit1')
        patch = self.scm.create_patch()
        patch_with_changed_files = self.scm.create_patch(changed_files=['test_file_commit1', 'test_file_commit2'])
        self.assertEqual(patch, patch_with_changed_files)

    def test_create_patch_git_commit(self):
        self._two_local_commits()
        patch = self.scm.create_patch(git_commit="HEAD^")
        self.assertRegexpMatches(patch, r'test_file_commit1')
        self.assertNotRegexpMatches(patch, r'test_file_commit2')

    def test_create_patch_git_commit_range(self):
        self._three_local_commits()
        patch = self.scm.create_patch(git_commit="HEAD~2..HEAD")
        self.assertNotRegexpMatches(patch, r'test_file_commit0')
        self.assertRegexpMatches(patch, r'test_file_commit2')
        self.assertRegexpMatches(patch, r'test_file_commit1')

    def test_create_patch_working_copy_only(self):
        self._one_local_commit_plus_working_copy_changes()
        patch = self.scm.create_patch(git_commit="HEAD....")
        self.assertNotRegexpMatches(patch, r'test_file_commit1')
        self.assertRegexpMatches(patch, r'test_file_commit2')

    def test_create_patch_multiple_local_commits(self):
        self._two_local_commits()
        patch = self.scm.create_patch()
        self.assertRegexpMatches(patch, r'test_file_commit2')
        self.assertRegexpMatches(patch, r'test_file_commit1')

    def test_create_patch_not_synced(self):
        run_command(['git', 'checkout', '-b', 'my-branch', 'trunk~3'])
        self._two_local_commits()
        patch = self.scm.create_patch()
        self.assertNotRegexpMatches(patch, r'test_file2')
        self.assertRegexpMatches(patch, r'test_file_commit2')
        self.assertRegexpMatches(patch, r'test_file_commit1')

    def test_create_binary_patch(self):
        # Create a git binary patch and check the contents.
        test_file_name = 'binary_file'
        test_file_path = os.path.join(self.git_checkout_path, test_file_name)
        file_contents = ''.join(map(chr, range(256)))
        write_into_file_at_path(test_file_path, file_contents, encoding=None)
        run_command(['git', 'add', test_file_name])
        patch = self.scm.create_patch()
        self.assertRegexpMatches(patch, r'\nliteral 0\n')
        self.assertRegexpMatches(patch, r'\nliteral 256\n')

        # Check if we can create a patch from a local commit.
        write_into_file_at_path(test_file_path, file_contents, encoding=None)
        run_command(['git', 'add', test_file_name])
        run_command(['git', 'commit', '-m', 'binary diff'])

        patch_from_local_commit = self.scm.create_patch('HEAD')
        self.assertRegexpMatches(patch_from_local_commit, r'\nliteral 0\n')
        self.assertRegexpMatches(patch_from_local_commit, r'\nliteral 256\n')


    def test_changed_files_local_plus_working_copy(self):
        self._one_local_commit_plus_working_copy_changes()
        files = self.scm._changed_files()
        self.assertIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)

        # working copy should *not* be in the list.
        files = self.scm._changed_files('trunk..')
        self.assertIn('test_file_commit1', files)
        self.assertNotIn('test_file_commit2', files)

        # working copy *should* be in the list.
        files = self.scm._changed_files('trunk....')
        self.assertIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)

    def test_changed_files_git_commit(self):
        self._two_local_commits()
        files = self.scm._changed_files(git_commit="HEAD^")
        self.assertIn('test_file_commit1', files)
        self.assertNotIn('test_file_commit2', files)

    def test_changed_files_git_commit_range(self):
        self._three_local_commits()
        files = self.scm._changed_files(git_commit="HEAD~2..HEAD")
        self.assertNotIn('test_file_commit0', files)
        self.assertIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)

    def test_changed_files_working_copy_only(self):
        self._one_local_commit_plus_working_copy_changes()
        files = self.scm._changed_files(git_commit="HEAD....")
        self.assertNotIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)

    def test_changed_files_multiple_local_commits(self):
        self._two_local_commits()
        files = self.scm._changed_files()
        self.assertIn('test_file_commit2', files)
        self.assertIn('test_file_commit1', files)

    def test_changed_files_not_synced(self):
        run_command(['git', 'checkout', '-b', 'my-branch', 'trunk~3'])
        self._two_local_commits()
        files = self.scm._changed_files()
        self.assertNotIn('test_file2', files)
        self.assertIn('test_file_commit2', files)
        self.assertIn('test_file_commit1', files)

    def test_changed_files_upstream(self):
        run_command(['git', 'checkout', '-t', '-b', 'my-branch'])
        self._one_local_commit()
        run_command(['git', 'checkout', '-t', '-b', 'my-second-branch'])
        self._second_local_commit()
        write_into_file_at_path('test_file_commit0', 'more test content')
        run_command(['git', 'add', 'test_file_commit0'])

        # equivalent to 'git diff my-branch..HEAD, should not include working changes
        files = self.scm._changed_files(git_commit='UPSTREAM..')
        self.assertNotIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)
        self.assertNotIn('test_file_commit0', files)

        # equivalent to 'git diff my-branch', *should* include working changes
        files = self.scm._changed_files(git_commit='UPSTREAM....')
        self.assertNotIn('test_file_commit1', files)
        self.assertIn('test_file_commit2', files)
        self.assertIn('test_file_commit0', files)

    def test_add_recursively(self):
        self._shared_test_add_recursively()

    def test_delete(self):
        self._two_local_commits()
        self.scm.delete('test_file_commit1')
        self.assertIn("test_file_commit1", self.scm._deleted_files())

    def test_delete_list(self):
        self._two_local_commits()
        self.scm.delete_list(["test_file_commit1", "test_file_commit2"])
        self.assertIn("test_file_commit1", self.scm._deleted_files())
        self.assertIn("test_file_commit2", self.scm._deleted_files())

    def test_delete_recursively(self):
        self._shared_test_delete_recursively()

    def test_delete_recursively_or_not(self):
        self._shared_test_delete_recursively_or_not()

    def test_move(self):
        self._shared_test_move()

    def test_move_recursive(self):
        self._shared_test_move_recursive()

    def test_exists(self):
        self._shared_test_exists(self.scm, self.scm.commit_locally_with_message)


# We need to split off more of these SCM tests to use mocks instead of the filesystem.
# This class is the first part of that.
class GitTestWithMock(unittest.TestCase):
    maxDiff = None

    def make_scm(self, logging_executive=False):
        # We do this should_log dance to avoid logging when Git.__init__ runs sysctl on mac to check for 64-bit support.
        scm = Git(cwd=".", executive=MockExecutive(), filesystem=MockFileSystem())
        scm.read_git_config = lambda *args, **kw: "MOCKKEY:MOCKVALUE"
        scm._executive._should_log = logging_executive
        return scm

    def test_timestamp_of_revision(self):
        scm = self.make_scm()
        scm.find_checkout_root = lambda path: ''
        scm._run_git = lambda args: 'Date: 2013-02-08 08:05:49 +0000'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T08:05:49Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:02:03 +0130'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-07T23:32:03Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:55:21 -0800'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T09:55:21Z')

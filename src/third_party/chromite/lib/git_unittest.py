# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from __future__ import print_function

import errno
import functools
import os

import mock

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import patch_unittest


class ManifestMock(partial_mock.PartialMock):
  """Partial mock for git.Manifest."""
  TARGET = 'chromite.lib.git.Manifest'
  ATTRS = ('_RunParser',)

  def _RunParser(self, *_args):
    pass


class ManifestCheckoutMock(partial_mock.PartialMock):
  """Partial mock for git.ManifestCheckout."""
  TARGET = 'chromite.lib.git.ManifestCheckout'
  ATTRS = ('_GetManifestsBranch',)

  def _GetManifestsBranch(self, _root):
    return 'default'


class NormalizeRefTest(cros_test_lib.TestCase):
  """Test the Normalize*Ref functions."""

  def _TestNormalize(self, functor, tests):
    """Helper function for testing Normalize*Ref functions.

    Args:
      functor: Normalize*Ref functor that only needs the input
        ref argument.
      tests: Dict of test inputs to expected test outputs.
    """
    for test_input, test_output in tests.items():
      result = functor(test_input)
      msg = ('Expected %s to translate %r to %r, but got %r.' %
             (functor.__name__, test_input, test_output, result))
      self.assertEqual(test_output, result, msg)

  def testNormalizeRef(self):
    """Test git.NormalizeRef function."""
    tests = {
        # These should all get 'refs/heads/' prefix.
        'foo': 'refs/heads/foo',
        'foo-bar-123': 'refs/heads/foo-bar-123',

        # If input starts with 'refs/' it should be left alone.
        'refs/foo/bar': 'refs/foo/bar',
        'refs/heads/foo': 'refs/heads/foo',

        # Plain 'refs' is nothing special.
        'refs': 'refs/heads/refs',

        None: None,
    }
    self._TestNormalize(git.NormalizeRef, tests)

  def testNormalizeRemoteRef(self):
    """Test git.NormalizeRemoteRef function."""
    remote = 'TheRemote'
    tests = {
        # These should all get 'refs/remotes/TheRemote' prefix.
        'foo': 'refs/remotes/%s/foo' % remote,
        'foo-bar-123': 'refs/remotes/%s/foo-bar-123' % remote,

        # These should be translated from local to remote ref.
        'refs/heads/foo': 'refs/remotes/%s/foo' % remote,
        'refs/heads/foo-bar-123': 'refs/remotes/%s/foo-bar-123' % remote,

        # These should be moved from one remote to another.
        'refs/remotes/OtherRemote/foo': 'refs/remotes/%s/foo' % remote,

        # These should be left alone.
        'refs/remotes/%s/foo' % remote: 'refs/remotes/%s/foo' % remote,
        'refs/foo/bar': 'refs/foo/bar',

        # Plain 'refs' is nothing special.
        'refs': 'refs/remotes/%s/refs' % remote,

        None: None,
    }

    # Add remote arg to git.NormalizeRemoteRef.
    functor = functools.partial(git.NormalizeRemoteRef, remote)
    functor.__name__ = git.NormalizeRemoteRef.__name__

    self._TestNormalize(functor, tests)


class GitWrappersTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for small git wrappers"""

  CHANGE_ID = 'I0da12ef6d2c670305f0281641bc53db22faf5c1a'
  COMMIT_LOG = """
foo: Change to foo.

Change-Id: %s
""" % CHANGE_ID

  PUSH_REMOTE = 'fake_remote'
  PUSH_BRANCH = 'fake_branch'
  PUSH_LOCAL = 'fake_local_branch'

  def setUp(self):
    self.fake_git_dir = os.path.join(self.tempdir, 'foo/bar')
    self.fake_file = 'baz'
    self.fake_path = os.path.join(self.fake_git_dir, self.fake_file)

  def testInit(self):
    git.Init(self.fake_path)

    # Should have created the git repo directory, if it didn't exist.
    self.assertExists(self.fake_git_dir)
    self.assertCommandContains(['init'])

  def testClone(self):
    url = 'http://happy/git/repo'

    git.Clone(self.fake_git_dir, url)

    # Should have created the git repo directory, if it didn't exist.
    self.assertExists(self.fake_git_dir)
    self.assertCommandContains(['git', 'clone', url, self.fake_git_dir])

  def testCloneComplex(self):
    url = 'http://happy/git/repo'
    ref = 'other/git/repo'

    git.Clone(self.fake_git_dir, url,
              reference=ref, branch='feature', single_branch=True)

    self.assertCommandContains(['git', 'clone', url, self.fake_git_dir,
                                '--reference', ref,
                                '--branch', 'feature', '--single-branch'])

  def testShallowFetch(self):
    url = 'http://happy/git/repo'

    sparse_checkout = os.path.join(self.fake_git_dir,
                                   '.git', 'info', 'sparse-checkout')
    osutils.SafeMakedirs(os.path.dirname(sparse_checkout))

    git.ShallowFetch(self.fake_git_dir, url,
                     sparse_checkout=['dir1/file1', 'dir2/file2'])

    # Should have created the git repo directory, if it didn't exist.
    self.assertExists(self.fake_git_dir)
    self.assertCommandContains(['init'])
    self.assertCommandContains(['config', 'core.sparsecheckout', 'true'])
    self.assertCommandContains(['remote', 'add', 'origin', url])
    self.assertCommandContains(['fetch', '--depth=1'])
    self.assertCommandContains(['pull', 'origin', 'master'])
    self.assertEqual(osutils.ReadFile(sparse_checkout),
                     'dir1/file1\ndir2/file2')

  def testFindGitTopLevel(self):
    git.FindGitTopLevel(self.fake_path)
    self.assertCommandContains(['--show-toplevel'])

  def testAddPath(self):
    git.AddPath(self.fake_path)
    self.assertCommandContains(['add'])
    self.assertCommandContains([self.fake_file])

  def testRmPath(self):
    git.RmPath(self.fake_path)
    self.assertCommandContains(['rm'])
    self.assertCommandContains([self.fake_file])

  def testGetObjectAtRev(self):
    git.GetObjectAtRev(self.fake_git_dir, '.', '1234')
    self.assertCommandContains(['show'])

  def testRevertPath(self):
    git.RevertPath(self.fake_git_dir, self.fake_file, '1234')
    self.assertCommandContains(['checkout'])
    self.assertCommandContains([self.fake_file])

  def testCommit(self):
    self.rc.AddCmdResult(partial_mock.In('log'), output=self.COMMIT_LOG)
    git.Commit(self.fake_git_dir, 'bar')
    self.assertCommandContains(['--amend'], expected=False)
    cid = git.Commit(self.fake_git_dir, 'bar', amend=True)
    self.assertCommandContains(['--amend'])
    self.assertCommandContains(['--allow-empty'], expected=False)
    self.assertEqual(cid, self.CHANGE_ID)
    cid = git.Commit(self.fake_git_dir, 'new', allow_empty=True)
    self.assertCommandContains(['--allow-empty'])

  def testUploadCLNormal(self):
    git.UploadCL(self.fake_git_dir, self.PUSH_REMOTE, self.PUSH_BRANCH,
                 local_branch=self.PUSH_LOCAL)
    self.assertCommandContains(['%s:refs/for/%s' % (self.PUSH_LOCAL,
                                                    self.PUSH_BRANCH)],
                               capture_output=False)

  def testUploadCLDraft(self):
    git.UploadCL(self.fake_git_dir, self.PUSH_REMOTE, self.PUSH_BRANCH,
                 local_branch=self.PUSH_LOCAL, draft=True)
    self.assertCommandContains(['%s:refs/drafts/%s' % (self.PUSH_LOCAL,
                                                       self.PUSH_BRANCH)],
                               capture_output=False)

  def testUploadCLCaptured(self):
    git.UploadCL(self.fake_git_dir, self.PUSH_REMOTE, self.PUSH_BRANCH,
                 local_branch=self.PUSH_LOCAL, draft=True, capture_output=True)
    self.assertCommandContains(['%s:refs/drafts/%s' % (self.PUSH_LOCAL,
                                                       self.PUSH_BRANCH)],
                               capture_output=True)

  def testGetGitRepoRevision(self):
    git.GetGitRepoRevision(self.fake_git_dir)
    self.assertCommandContains(['rev-parse', 'HEAD'])
    git.GetGitRepoRevision(self.fake_git_dir, branch='branch')
    self.assertCommandContains(['rev-parse', 'branch'])
    git.GetGitRepoRevision(self.fake_git_dir, short=True)
    self.assertCommandContains(['rev-parse', '--short', 'HEAD'])
    git.GetGitRepoRevision(self.fake_git_dir, branch='branch', short=True)
    self.assertCommandContains(['rev-parse', '--short', 'branch'])

  def testGetGitGitdir(self):
    git.Init(self.fake_git_dir)
    os.makedirs(os.path.join(self.fake_git_dir, '.git', 'refs', 'heads'))
    os.makedirs(os.path.join(self.fake_git_dir, '.git', 'objects'))
    other_file = os.path.join(self.fake_git_dir, 'other_file')
    osutils.Touch(other_file)

    ret = git.GetGitGitdir(self.fake_git_dir)
    self.assertEqual(ret, os.path.join(self.fake_git_dir, '.git'))

  def testGetGitGitdir_bare(self):
    git.Init(self.fake_git_dir)
    os.makedirs(os.path.join(self.fake_git_dir, 'refs', 'heads'))
    os.makedirs(os.path.join(self.fake_git_dir, 'objects'))
    config_file = os.path.join(self.fake_git_dir, 'config')
    osutils.Touch(config_file)

    ret = git.GetGitGitdir(self.fake_git_dir)
    self.assertEqual(ret, self.fake_git_dir)

  def testGetGitGitdir_negative(self):
    ret = git.GetGitGitdir(self.tempdir)
    self.assertFalse(ret)

  def testDeleteStaleLocks(self):
    git.Init(self.fake_git_dir)
    refs_heads = os.path.join(self.fake_git_dir, '.git', 'refs', 'heads')
    os.makedirs(refs_heads)
    objects = os.path.join(self.fake_git_dir, '.git', 'objects')
    os.makedirs(objects)
    fake_lock = os.path.join(refs_heads, 'master.lock')
    osutils.Touch(fake_lock)
    os.makedirs(self.fake_path)
    dot_lock_not_in_dot_git = os.path.join(self.fake_git_dir, 'some.lock')
    osutils.Touch(dot_lock_not_in_dot_git)
    other_file = os.path.join(self.fake_path, 'other_file')
    osutils.Touch(other_file)

    git.DeleteStaleLocks(self.fake_git_dir)
    self.assertExists(os.path.join(self.fake_git_dir, '.git'))
    self.assertExists(refs_heads)
    self.assertExists(objects)
    self.assertExists(dot_lock_not_in_dot_git)
    self.assertExists(other_file)
    self.assertNotExists(fake_lock)

  def testDeleteStaleLocks_bare(self):
    git.Init(self.fake_git_dir)
    refs_heads = os.path.join(self.fake_git_dir, 'refs', 'heads')
    os.makedirs(refs_heads)
    objects = os.path.join(self.fake_git_dir, 'objects')
    os.makedirs(objects)
    fake_lock = os.path.join(refs_heads, 'master.lock')
    osutils.Touch(fake_lock)
    os.makedirs(self.fake_path)
    other_file = os.path.join(self.fake_path, 'other_file')
    osutils.Touch(other_file)

    git.DeleteStaleLocks(self.fake_git_dir)
    self.assertExists(refs_heads)
    self.assertExists(objects)
    self.assertExists(other_file)
    self.assertNotExists(fake_lock)


class LogTest(cros_test_lib.RunCommandTestCase):
  """Tests for git.Log"""

  def testNoArgs(self):
    git.Log('git/repo/path')
    self.assertCommandContains(['git', 'log'], cwd='git/repo/path')

  def testAllArgs(self):
    git.Log('git/repo/path', format='format:"%cd"',
            after='1996-01-01', until='1997-01-01', reverse=True,
            date='unix', max_count='1',
            grep='^Change-ID: I9f701664d849197cf183fc1fb46f7523095c359c$',
            rev='m/master', paths=['my/path'])
    self.assertCommandContains(
        [
            'git', 'log', '--format=format:"%cd"', '--after=1996-01-01',
            '--until=1997-01-01', '--reverse', '--date=unix', '--max-count=1',
            '--grep=^Change-ID: I9f701664d849197cf183fc1fb46f7523095c359c$',
            'm/master',
            '--', 'my/path',
        ], cwd='git/repo/path')


class ChangeIdTest(cros_test_lib.MockTestCase):
  """Tests for git.GetChangeId function."""

  def testHEAD(self):
    """Test the parsing of the git.GetChangeId function for HEAD."""

    log_output = """
lib/git: break out ChangeId into its own function

Code in Commit() will get the Change-Id after doing a git commit,
but in some use cases, we want to get the Change-Id of a commit
that already exists, without changing it. Move this code into its
own function that Commit() calls or an external user can call it
directly.

BUG=None
TEST=Start python3
>>> from chromite.lib import git
>>> print(git.GetChangeId('.'))
>>> exit(0)
$ git show
Compare the Change-Id printed by the python code with that shown

Change-Id: Ia7b712c42ff83c52c0fb5d88d1ef6c62f49da88d
"""
    result = cros_build_lib.CommandResult(output=log_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    changeid = git.GetChangeId('git/repo/path')
    self.assertEqual(changeid, 'Ia7b712c42ff83c52c0fb5d88d1ef6c62f49da88d')

  def testSpecificSHA(self):
    """Test the parsing of the git.GetChangeId function for a specific SHA."""

    sha = '235511fbd7158c6d02c070944eb59cf47b37fcb5'
    log_output = """
Add user cros-disks to group android-everybody

This allows cros-disks to access 'Play Files'.

BUG=chromium:996549
TEST=Manually built and inspected group file

Cq-Depend: chromium:2032906
Change-Id: Id31c1211f95d7f5c3a94fbe8c028f65d3509f363
Reviewed-on: https://chromium-review.googlesource.com/c/chromiumos/chromite/+/2040633
Reviewed-by: Mike Frysinger <vapier@chromium.org>
Commit-Queue: François Degros <fdegros@chromium.org>
Tested-by: François Degros <fdegros@chromium.org>
"""
    result = cros_build_lib.CommandResult(output=log_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    changeid = git.GetChangeId('git/repo/path', sha)
    self.assertEqual(changeid, 'Id31c1211f95d7f5c3a94fbe8c028f65d3509f363')

  def testNoChangeId(self):
    """Test git.GetChangeId function if there is no Change-Id."""

    log_output = """
lib/git: break out ChangeId into its own function

Code in Commit() will get the Change-Id after doing a git commit,
but in some use cases, we want to get the Change-Id of a commit
that already exists, without changing it. Move this code into its
own function that Commit() calls or an external user can call it
directly.

BUG=None
TEST=Start python3
>>> from chromite.lib import git
>>> print(git.GetChangeId('.'))
>>> exit(0)
$ git show
Compare the Change-Id printed by the python code with that shown
"""
    result = cros_build_lib.CommandResult(output=log_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    changeid = git.GetChangeId('git/repo/path')
    self.assertIsNone(changeid)

  def testChangeIdInTextCol1(self):
    """Test git.GetChangeId when 'Change-Id' is in the text, not as a key."""

    log_output = """
new_variant: track branch name and change-id

new_variant.py calls several scripts to create a new variant of a
reference board. Each of these scripts adds or modifies files and
creates a new commit. Track the branch name and change-id of each
commit in preparation for uploading to gerrit.
This CL builds on the changes in
Change-Id: I53af157625257ee1ecf39a4ced979138890b54f1
and while I would normally use a relation chain or cq-depend, I'm
putting the text directly in here so that the unit test will fail
until I fix the code to handle the pathological cases.

Cq-Depend: chromium:2041804
Change-Id: Ib71696f76dc80f1a76b8e7a73493c6c2668e2c6f
"""
    result = cros_build_lib.CommandResult(output=log_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    self.assertRaises(ValueError, git.GetChangeId, 'git/repo/path')

  def testChangeIdInTextNotCol1(self):
    """Test git.GetChangeId when 'Change-Id' is in the text, not as a key."""

    log_output = """
new_variant: track branch name and change-id

new_variant.py calls several scripts to create a new variant of a
reference board. Each of these scripts adds or modifies files and
creates a new commit. Track the branch name and change-id of each
commit in preparation for uploading to gerrit. This CL builds on the
changes in Change-Id: I53af157625257ee1ecf39a4ced979138890b54f1
and while I would normally use a relation chain or cq-depend, I'm
putting the text directly in here so that the unit test will fail
until I fix the code to handle the pathological cases.

Cq-Depend: chromium:2041804
Change-Id: Ib71696f76dc80f1a76b8e7a73493c6c2668e2c6f
"""
    result = cros_build_lib.CommandResult(output=log_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    changeid = git.GetChangeId('git/repo/path')
    self.assertEqual(changeid, 'Ib71696f76dc80f1a76b8e7a73493c6c2668e2c6f')


class ProjectCheckoutTest(cros_test_lib.TestCase):
  """Tests for git.ProjectCheckout"""

  def setUp(self):
    self.fake_unversioned_patchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='remotes/for/master'))
    self.fake_unversioned_unpatchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/platform/somethingsomething/chromite',
             # Pinned to a SHA1.
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf'))
    self.fake_versioned_patchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf',
             upstream='remotes/for/master'))
    self.fake_versioned_unpatchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf',
             upstream='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf'))


class RawDiffTest(cros_test_lib.MockTestCase):
  """Tests for git.RawDiff function."""

  def testRawDiff(self):
    """Test the parsing of the git.RawDiff function."""

    diff_output = """
:100644 100644 ac234b2... 077d1f8... M\tchromeos-base/chromeos-chrome/Manifest
:100644 100644 9e5d11b... 806bf9b... R099\tchromeos-base/chromeos-chrome/chromeos-chrome-40.0.2197.0_rc-r1.ebuild\tchromeos-base/chromeos-chrome/chromeos-chrome-40.0.2197.2_rc-r1.ebuild
:100644 100644 70d6e94... 821c642... M\tchromeos-base/chromeos-chrome/chromeos-chrome-9999.ebuild
:100644 100644 be445f9... be445f9... R100\tchromeos-base/chromium-source/chromium-source-40.0.2197.0_rc-r1.ebuild\tchromeos-base/chromium-source/chromium-source-40.0.2197.2_rc-r1.ebuild
"""
    result = cros_build_lib.CommandResult(output=diff_output)
    self.PatchObject(git, 'RunGit', return_value=result)

    entries = git.RawDiff('foo', 'bar')
    self.assertEqual(entries, [
        ('100644', '100644', 'ac234b2', '077d1f8', 'M', None,
         'chromeos-base/chromeos-chrome/Manifest', None),
        ('100644', '100644', '9e5d11b', '806bf9b', 'R', '099',
         'chromeos-base/chromeos-chrome/'
         'chromeos-chrome-40.0.2197.0_rc-r1.ebuild',
         'chromeos-base/chromeos-chrome/'
         'chromeos-chrome-40.0.2197.2_rc-r1.ebuild'),
        ('100644', '100644', '70d6e94', '821c642', 'M', None,
         'chromeos-base/chromeos-chrome/chromeos-chrome-9999.ebuild', None),
        ('100644', '100644', 'be445f9', 'be445f9', 'R', '100',
         'chromeos-base/chromium-source/'
         'chromium-source-40.0.2197.0_rc-r1.ebuild',
         'chromeos-base/chromium-source/'
         'chromium-source-40.0.2197.2_rc-r1.ebuild')
    ])

  def testEmptyDiff(self):
    """Verify an empty diff doesn't crash."""
    result = cros_build_lib.CommandResult(output='\n')
    self.PatchObject(git, 'RunGit', return_value=result)
    entries = git.RawDiff('foo', 'bar')
    self.assertEqual([], entries)


class GitPushTest(cros_test_lib.RunCommandTestCase):
  """Tests for git.GitPush function."""

  # Non fast-forward push error message.
  NON_FF_PUSH_ERROR = (
      'To https://localhost/repo.git\n'
      '! [remote rejected] master -> master (non-fast-forward)\n'
      "error: failed to push some refs to 'https://localhost/repo.git'\n")

  # List of possible GoB transient errors.
  TRANSIENT_ERRORS = (
      # Hook error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: (-)To https://localhost/repo.git\n'
       '! [remote rejected] 6c78ca083c3a9d64068c945fd9998eb1e0a3e739 -> '
       'stabilize-4636.B (error in hook)\n'
       "error: failed to push some refs to 'https://localhost/repo.git'\n"),

      # 'failed to lock' error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: done\nTo https://localhost/repo.git\n'
       '! [remote rejected] 4ea09c129b5fedb261bae2431ce2511e35ac3923 -> '
       'stabilize-daisy-4319.96.B (failed to lock)\n'
       "error: failed to push some refs to 'https://localhost/repo.git'\n"),

      # Hook error when pushing branch.
      ('remote: Processing changes: (\\)To https://localhost/repo.git\n'
       '! [remote rejected] temp_auto_checkin_branch -> '
       'master (error in hook)\n'
       "error: failed to push some refs to 'https://localhost/repo.git'\n"),

      # Another kind of error when pushing a branch.
      'fatal: remote error: Internal Server Error',

      # crbug.com/298189
      ('error: gnutls_handshake() failed: A TLS packet with unexpected length '
       'was received. while accessing '
       'http://localhost/repo.git/info/refs?service=git-upload-pack\n'
       'fatal: HTTP request failed'),

      # crbug.com/298189
      ("fatal: unable to access 'https://localhost/repo.git': GnuTLS recv "
       'error (-9): A TLS packet with unexpected length was received.'),
  )

  def setUp(self):
    self.StartPatcher(mock.patch('time.sleep'))

  @staticmethod
  def _RunGitPush():
    """Runs git.GitPush with some default arguments."""
    git.GitPush('some_repo_path', 'local-ref',
                git.RemoteRef('some-remote', 'remote-ref'))

  def testGitPushSimple(self):
    """Test GitPush with minimal arguments."""
    git.GitPush('git_path', 'HEAD', git.RemoteRef('origin', 'master'))
    self.assertCommandCalled(['git', 'push', 'origin', 'HEAD:master'],
                             capture_output=True, print_cmd=False,
                             cwd='git_path', encoding='utf-8')

  def testGitPushComplix(self):
    """Test GitPush with some arguments."""
    git.GitPush('git_path', 'HEAD', git.RemoteRef('origin', 'master'),
                force=True, dry_run=True)
    self.assertCommandCalled(['git', 'push', 'origin', 'HEAD:master',
                              '--force', '--dry-run'],
                             capture_output=True, print_cmd=False,
                             cwd='git_path', encoding='utf-8')

  def testNonFFPush(self):
    """Non fast-forward push error propagates to the caller."""
    self.rc.AddCmdResult(partial_mock.In('push'), returncode=128,
                         error=self.NON_FF_PUSH_ERROR)
    self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)

  def testPersistentTransientError(self):
    """GitPush fails if transient error occurs multiple times."""
    for error in self.TRANSIENT_ERRORS:
      self.rc.AddCmdResult(partial_mock.In('push'), returncode=128,
                           error=error)
      self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)


class GitIntegrationTest(patch_unittest.GitRepoPatchTestCase):
  """Tests that git library functions work with actual git repos."""

  def testIsReachableTrue(self):
    git1 = self._MakeRepo('git1', self.source)
    patch1 = self.CommitFile(git1, 'foo', 'foo')
    patch2 = self.CommitFile(git1, 'bar', 'bar')
    self.assertTrue(git.IsReachable(git1, patch1.sha1, patch2.sha1))

  def testIsReachableFalse(self):
    git1 = self._MakeRepo('git1', self.source)
    patch1 = self.CommitFile(git1, 'foo', 'foo')
    patch2 = self.CommitFile(git1, 'bar', 'bar')
    self.assertFalse(git.IsReachable(git1, patch2.sha1, patch1.sha1))

  def testDoesCommitExistInRepoWithAmbiguousBranchName(self):
    git1 = self._MakeRepo('git1', self.source)
    git.CreateBranch(git1, 'peach', track=True)
    self.CommitFile(git1, 'peach', 'Keep me.')
    self.assertTrue(git.DoesCommitExistInRepo(git1, 'peach'))


class ManifestCheckoutTest(cros_test_lib.TempDirTestCase):
  """Tests for ManifestCheckout functionality."""

  def setUp(self):
    self.manifest_dir = os.path.join(self.tempdir, '.repo', 'manifests')

    # Initialize a repo instance here.
    local_repo = os.path.join(constants.SOURCE_ROOT, '.repo/repo/.git')

    # TODO(evanhernandez): This is a hack. Find a way to simplify this test.
    # We used to use the current checkout's manifests.git, but that caused
    # problems in production environemnts.
    remote_manifests = os.path.join(self.tempdir, 'remote', 'manifests.git')
    osutils.SafeMakedirs(remote_manifests)
    git.Init(remote_manifests)
    default_manifest = os.path.join(remote_manifests, 'default.xml')
    osutils.WriteFile(
        default_manifest,
        '<?xml version="1.0" encoding="UTF-8"?><manifest></manifest>')
    git.AddPath(default_manifest)
    git.Commit(remote_manifests, 'dummy commit', allow_empty=True)
    git.CreateBranch(remote_manifests, 'default')
    git.CreateBranch(remote_manifests, 'release-R23-2913.B')
    git.CreateBranch(remote_manifests, 'release-R23-2913.B-suffix')
    git.CreateBranch(remote_manifests, 'firmware-link-')

    # Create a copy of our existing manifests.git, but rewrite it so it
    # looks like a remote manifests.git.  This is to avoid hitting the
    # network, and speeds things up in general.
    local_manifests = 'file://%s' % remote_manifests
    temp_manifests = os.path.join(self.tempdir, 'manifests.git')
    git.RunGit(self.tempdir, ['clone', '-n', '--bare', local_manifests])
    git.RunGit(temp_manifests,
               ['fetch', '-f', '-u', local_manifests,
                'refs/remotes/origin/*:refs/heads/*'])
    git.RunGit(temp_manifests, ['branch', '-D', 'default'])
    cros_build_lib.run([
        'repo', 'init', '-u', temp_manifests,
        '--repo-branch', 'default', '--repo-url', 'file://%s' % local_repo,
    ], cwd=self.tempdir)

    self.active_manifest = os.path.realpath(
        os.path.join(self.tempdir, '.repo', 'manifest.xml'))

  def testManifestInheritance(self):
    osutils.WriteFile(self.active_manifest, """
        <manifest>
          <include name="include-target.xml" />
          <include name="empty.xml" />
          <project name="monkeys" path="baz" remote="foon" revision="master" />
        </manifest>""")
    # First, verify it properly explodes if the include can't be found.
    self.assertRaises(EnvironmentError,
                      git.ManifestCheckout, self.tempdir)

    # Next, verify it can read an empty manifest; this is to ensure
    # that we can point Manifest at the empty manifest without exploding,
    # same for ManifestCheckout; this sort of thing is primarily useful
    # to ensure no step of an include assumes everything is yet assembled.
    empty_path = os.path.join(self.manifest_dir, 'empty.xml')
    osutils.WriteFile(empty_path, '<manifest/>')
    git.Manifest(empty_path)
    git.ManifestCheckout(self.tempdir, manifest_path=empty_path)

    # Next, verify include works.
    osutils.WriteFile(
        os.path.join(self.manifest_dir, 'include-target.xml'),
        """
        <manifest>
          <remote name="foon" fetch="http://localhost" />
        </manifest>""")
    manifest = git.ManifestCheckout(self.tempdir)
    self.assertEqual(list(manifest.checkouts_by_name), ['monkeys'])
    self.assertEqual(list(manifest.remotes), ['foon'])

  def testGetManifestsBranch(self):
    # pylint: disable=protected-access
    func = git.ManifestCheckout._GetManifestsBranch
    manifest = self.manifest_dir
    repo_root = self.tempdir

    # pylint: disable=unused-argument
    def reconfig(merge='master', origin='origin'):
      if merge is not None:
        merge = 'refs/heads/%s' % merge
      for key in ('merge', 'origin'):
        val = locals()[key]
        key = 'branch.default.%s' % key
        if val is None:
          git.RunGit(manifest, ['config', '--unset', key], check=False)
        else:
          git.RunGit(manifest, ['config', key, val])

    # First, verify our assumptions about a fresh repo init are correct.
    self.assertEqual('default', git.GetCurrentBranch(manifest))
    self.assertEqual('master', func(repo_root))

    # Ensure we can handle a missing origin; this can occur jumping between
    # branches, and can be worked around.
    reconfig(origin=None)
    self.assertEqual('default', git.GetCurrentBranch(manifest))
    self.assertEqual('master', func(repo_root))

    def assertExcept(message, **kwargs):
      reconfig(**kwargs)
      self.assertRaises2(OSError, func, repo_root, ex_msg=message,
                         check_attrs={'errno': errno.ENOENT})

    # No merge target means the configuration isn't usable, period.
    assertExcept('git tracking configuration for that branch is broken',
                 merge=None)

    # Ensure we detect if we're on the wrong branch, even if it has
    # tracking setup.
    git.RunGit(manifest, ['checkout', '-t', 'origin/master', '-b', 'test'])
    assertExcept("It should be checked out to 'default'")

    # Ensure we handle detached HEAD w/ an appropriate exception.
    git.RunGit(manifest, ['checkout', '--detach', 'test'])
    assertExcept("It should be checked out to 'default'")

    # Finally, ensure that if the default branch is non-existant, we still throw
    # a usable exception.
    git.RunGit(manifest, ['branch', '-d', 'default'])
    assertExcept("It should be checked out to 'default'")

  def testGitMatchBranchName(self):
    git_repo = os.path.join(self.tempdir, '.repo', 'manifests')

    branches = git.MatchBranchName(git_repo, 'default', namespace='')
    self.assertEqual(branches, ['refs/heads/default'])

    branches = git.MatchBranchName(git_repo, 'default', namespace='refs/heads/')
    self.assertEqual(branches, ['default'])

    branches = git.MatchBranchName(git_repo, 'origin/f.*link',
                                   namespace='refs/remotes/')
    self.assertTrue('firmware-link-' in branches[0])

    branches = git.MatchBranchName(git_repo, 'r23')
    self.assertEqual(branches,
                     ['refs/remotes/origin/release-R23-2913.B',
                      'refs/remotes/origin/release-R23-2913.B-suffix'])

    branches = git.MatchBranchName(git_repo, 'release-R23-2913.B')
    self.assertEqual(branches, ['refs/remotes/origin/release-R23-2913.B'])

    branches = git.MatchBranchName(git_repo, 'release-R23-2913.B',
                                   namespace='refs/remotes/origin/')
    self.assertEqual(branches, ['release-R23-2913.B'])

    branches = git.MatchBranchName(git_repo, 'release-R23-2913.B',
                                   namespace='refs/remotes/')
    self.assertEqual(branches, ['origin/release-R23-2913.B'])

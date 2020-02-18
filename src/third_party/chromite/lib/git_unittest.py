# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from __future__ import print_function

import functools
import mock
import os

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
      self.assertEquals(test_output, result, msg)

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
  COMMIT_LOG = '''
  foo: Change to foo.

  Change-Id: %s
  ''' % CHANGE_ID

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
    self.assertEquals(osutils.ReadFile(sparse_checkout),
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
    self.assertEqual(ret, os.path.join(self.fake_git_dir, ".git"))

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
    git.Log('git/repo/path', pretty='format:"%cd"',
            after='1996-01-01', until='1997-01-01', reverse=True,
            date='unix', paths=['my/path'])
    self.assertCommandContains(
        [
            'git', 'log', '--pretty=format:"%cd"', '--after=1996-01-01',
            '--until=1997-01-01', '--reverse', '--date=unix',
            '--', 'my/path',
        ], cwd='git/repo/path')


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

    diff_output = '''
:100644 100644 ac234b2... 077d1f8... M\tchromeos-base/chromeos-chrome/Manifest
:100644 100644 9e5d11b... 806bf9b... R099\tchromeos-base/chromeos-chrome/chromeos-chrome-40.0.2197.0_rc-r1.ebuild\tchromeos-base/chromeos-chrome/chromeos-chrome-40.0.2197.2_rc-r1.ebuild
:100644 100644 70d6e94... 821c642... M\tchromeos-base/chromeos-chrome/chromeos-chrome-9999.ebuild
:100644 100644 be445f9... be445f9... R100\tchromeos-base/chromium-source/chromium-source-40.0.2197.0_rc-r1.ebuild\tchromeos-base/chromium-source/chromium-source-40.0.2197.2_rc-r1.ebuild
'''
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


class GitPushTest(cros_test_lib.RunCommandTestCase):
  """Tests for git.GitPush function."""

  # Non fast-forward push error message.
  NON_FF_PUSH_ERROR = (
      'To https://localhost/repo.git\n'
      '! [remote rejected] master -> master (non-fast-forward)\n'
      'error: failed to push some refs to \'https://localhost/repo.git\'\n')

  # List of possible GoB transient errors.
  TRANSIENT_ERRORS = (
      # Hook error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: (-)To https://localhost/repo.git\n'
       '! [remote rejected] 6c78ca083c3a9d64068c945fd9998eb1e0a3e739 -> '
       'stabilize-4636.B (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # 'failed to lock' error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: done\nTo https://localhost/repo.git\n'
       '! [remote rejected] 4ea09c129b5fedb261bae2431ce2511e35ac3923 -> '
       'stabilize-daisy-4319.96.B (failed to lock)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Hook error when pushing branch.
      ('remote: Processing changes: (\\)To https://localhost/repo.git\n'
       '! [remote rejected] temp_auto_checkin_branch -> '
       'master (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Another kind of error when pushing a branch.
      'fatal: remote error: Internal Server Error',

      # crbug.com/298189
      ('error: gnutls_handshake() failed: A TLS packet with unexpected length '
       'was received. while accessing '
       'http://localhost/repo.git/info/refs?service=git-upload-pack\n'
       'fatal: HTTP request failed'),

      # crbug.com/298189
      ('fatal: unable to access \'https://localhost/repo.git\': GnuTLS recv '
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
                             cwd='git_path')

  def testGitPushComplix(self):
    """Test GitPush with some arguments."""
    git.GitPush('git_path', 'HEAD', git.RemoteRef('origin', 'master'),
                force=True, dry_run=True)
    self.assertCommandCalled(['git', 'push', 'origin', 'HEAD:master',
                              '--force', '--dry-run'],
                             capture_output=True, print_cmd=False,
                             cwd='git_path')

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

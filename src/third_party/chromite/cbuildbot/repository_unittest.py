# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for repository.py."""

from __future__ import print_function

import os
import time
import mock

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.cbuildbot import repository
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import cros_build_lib

# pylint: disable=protected-access


class RepositoryTests(cros_test_lib.RunCommandTestCase):
  """Test cases related to repository checkout methods."""

  def testExternalRepoCheckout(self):
    """Test we detect external checkouts properly."""
    tests = [
        'https://chromium.googlesource.com/chromiumos/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest',
        'test@abcdef.bla.com:39291/bla/Manifest-internal',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertFalse(repository.IsInternalRepoCheckout('.'))

  def testInternalRepoCheckout(self):
    """Test we detect internal checkouts properly."""
    tests = [
        'https://chrome-internal.googlesource.com/chromeos/manifest-internal',
        'test@abcdef.bla.com:39291/bla/manifest-internal.git',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertTrue(repository.IsInternalRepoCheckout('.'))

  def testIsLocalPath(self):
    """test IsLocalPath."""
    self.assertTrue(repository._IsLocalPath('/tmp/chromiumos/'))
    self.assertTrue(repository._IsLocalPath('file:///chromiumos/'))
    self.assertFalse(repository._IsLocalPath('https://chromiumos/'))
    self.assertFalse(repository._IsLocalPath('http://chromiumos/'))
    self.assertFalse(repository._IsLocalPath('ssh://chromiumos/'))
    self.assertFalse(repository._IsLocalPath('git://chromiumos/'))


class RepoInitTests(cros_test_lib.TempDirTestCase, cros_test_lib.MockTestCase):
  """Test cases related to repository initialization."""

  def __init__(self, *args, **kwargs):
    self.repo = None
    super(RepoInitTests, self).__init__(*args, **kwargs)

  def setUp(self):
    self.PatchObject(time, 'sleep')

  def _Initialize(self, branch='master'):
    site_params = config_lib.GetSiteParams()
    self.repo = repository.RepoRepository(site_params.MANIFEST_URL,
                                          self.tempdir, branch=branch)
    self.repo.Initialize()

  @cros_test_lib.NetworkTest()
  def testReInitialization(self):
    """Test ability to switch between branches."""
    self._Initialize('release-R19-2046.B')
    self._Initialize('master')

    # Test that a failed re-init due to bad branch doesn't leave repo in bad
    # state.
    # repo init on 'monkey' will retry on failures.
    self.assertRaises(Exception, self._Initialize, 'monkey')
    self._Initialize('release-R20-2268.B')

  @cros_test_lib.NetworkTest()
  def testBuildRootGitCleanup(self):
    """Test successful repo cleanup."""
    self._Initialize()
    run_cmd_mock = self.PatchObject(
        cros_build_lib, 'RunCommand', wraps=cros_build_lib.RunCommand)
    self.repo.BuildRootGitCleanup(prune_all=True)

    # RunCommand should be called twice.
    self.assertEqual(run_cmd_mock.call_count, 2)

  @cros_test_lib.NetworkTest()
  def testCleanStaleLocks(self):
    """Test successful repo lock cleanup."""
    self._Initialize('release-R19-2046.B')
    self.PatchObject(git, 'GetGitGitdir')
    dsl = self.PatchObject(git, 'DeleteStaleLocks')
    self.repo.CleanStaleLocks()

    self.assertEqual(dsl.call_count, 198)

  def testInitializationWithRepoInitRetry(self):
    """Test Initialization with repo init retry."""
    self.PatchObject(repository.RepoRepository, '_RepoSelfupdate')
    mock_cleanup = self.PatchObject(repository.RepoRepository,
                                    '_CleanUpRepoManifest')
    error_result = cros_build_lib.CommandResult(cmd=['cmd'], returncode=1)
    ex = cros_build_lib.RunCommandError('error_msg', error_result)
    mock_init = self.PatchObject(cros_build_lib, 'RunCommand', side_effect=ex)

    self.assertRaises(Exception, self._Initialize)
    self.assertEqual(mock_cleanup.call_count,
                     repository.REPO_INIT_RETRY_LIMIT + 1)
    self.assertEqual(mock_init.call_count,
                     repository.REPO_INIT_RETRY_LIMIT + 1)

  def testInitializationWithoutRepoInitRetry(self):
    """Test Initialization without repo init retry."""
    self.PatchObject(repository.RepoRepository, '_RepoSelfupdate')
    mock_cleanup = self.PatchObject(repository.RepoRepository,
                                    '_CleanUpRepoManifest')
    mock_init = self.PatchObject(cros_build_lib, 'RunCommand')

    self._Initialize()
    self.assertEqual(mock_cleanup.call_count, 0)
    self.assertEqual(mock_init.call_count, 1)


class RepoInitChromeBotTests(RepoInitTests):
  """Test that Re-init works with the chrome-bot account.

  In testing, repo init behavior on the buildbots is different from a
  local run, because there is some logic in 'repo' that filters changes based
  on GIT_COMMITTER_IDENT.  So for sanity's sake, try to emulate running on the
  buildbots.
  """
  def setUp(self):
    os.putenv('GIT_COMMITTER_EMAIL', 'chrome-bot@chromium.org')
    os.putenv('GIT_AUTHOR_EMAIL', 'chrome-bot@chromium.org')


class RepoSyncTests(cros_test_lib.TempDirTestCase, cros_test_lib.MockTestCase):
  """Test cases related to repository Sync"""

  def setUp(self):
    site_params = config_lib.GetSiteParams()
    self.repo = repository.RepoRepository(site_params.MANIFEST_URL,
                                          self.tempdir, branch='master')
    self.PatchObject(repository.RepoRepository, 'Initialize')
    self.PatchObject(repository.RepoRepository, '_EnsureMirroring')
    self.PatchObject(repository.RepoRepository, 'BuildRootGitCleanup')
    self.PatchObject(time, 'sleep')

  def testSyncWithException(self):
    """Test Sync retry on repo network sync failure"""
    # Return value here isn't super important.
    result = cros_build_lib.CommandResult(
        cmd=['cmd'], returncode=0, error='error')
    ex = cros_build_lib.RunCommandError('msg', result)

    run_cmd_mock = self.PatchObject(cros_build_lib, 'RunCommand',
                                    side_effect=ex)

    # repo.Sync raises SrcCheckOutException.
    self.assertRaises(
        repository.SrcCheckOutException,
        self.repo.Sync, local_manifest='local_manifest', network_only=True)

    # RunCommand should be called SYNC_RETRIES + 1 times.
    self.assertEqual(run_cmd_mock.call_count,
                     constants.SYNC_RETRIES + 1)

  def testSyncWithoutException(self):
    """Test successful repo sync without exception and retry"""
    # Return value here isn't super important.
    run_cmd_mock = self.PatchObject(cros_build_lib, 'RunCommand')
    self.repo.Sync(local_manifest='local_manifest', network_only=True)

    # RunCommand should be called once.
    self.assertEqual(run_cmd_mock.call_count, 1)

  def test_RepoSelfupdateRaisesWarning(self):
    """Test _RepoSelfupdate when repo version warning is raised."""
    warnning_stderr = """
info: A new version of repo is available

...

gpg: Can't check signature: public key not found

...

warning: Skipped upgrade to unverified version
"""
    mock_rm = self.PatchObject(osutils, 'RmDir')
    cmd_result = cros_build_lib.CommandResult(error=warnning_stderr)
    self.PatchObject(cros_build_lib, 'RunCommand', return_value=cmd_result)
    self.repo._RepoSelfupdate()

    mock_rm.assert_called_once_with(mock.ANY, ignore_missing=True)

  def test_RepoSelfupdateRaisesException(self):
    """Test _RepoSelfupdate when exception is raised."""
    mock_rm = self.PatchObject(osutils, 'RmDir')
    ex = cros_build_lib.RunCommandError(
        'msg', cros_build_lib.CommandResult())
    self.PatchObject(cros_build_lib, 'RunCommand', side_effect=ex)
    self.repo._RepoSelfupdate()

    mock_rm.assert_called_once_with(mock.ANY, ignore_missing=True)

  def test_RepoSelfupdateSucceeds(self):
    mock_rm = self.PatchObject(osutils, 'RmDir')
    cmd_result = cros_build_lib.CommandResult()
    self.PatchObject(cros_build_lib, 'RunCommand', return_value=cmd_result)
    self.repo._RepoSelfupdate()

    self.assertFalse(mock_rm.called)

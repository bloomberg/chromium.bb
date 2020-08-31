# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for scripts/repo_sync_manifest."""

from __future__ import print_function

import os
import sys

import mock

from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.lib import cros_test_lib
from chromite.scripts import repo_sync_manifest


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class RepoSyncManifestTest(cros_test_lib.RunCommandTempDirTestCase):
  """Unit tests for repo_sync_manifest."""

  INT_MANIFEST_URL = ('https://chrome-internal-review.googlesource.com/'
                      'chromeos/manifest-internal')
  EXT_MANIFEST_URL = ('https://chromium.googlesource.com/chromiumos/manifest')

  MV_INT_URL = ('https://chrome-internal.googlesource.com/chromeos/'
                'manifest-versions')
  MV_EXT_URL = ('https://chromium.googlesource.com/chromiumos/'
                'manifest-versions')

  def setUp(self):
    self.repo_dir = os.path.join(self.tempdir, 'repo')
    self.repo_url = os.path.join(self.tempdir, '.repo/repo')
    self.preload_src = os.path.join(self.tempdir, 'source')
    self.git_cache = os.path.join(self.tempdir, 'git_cache')
    self.manifest = os.path.join(self.tempdir, 'manifest.xml')
    self.mv_ext = os.path.join(self.tempdir, 'mv_ext')
    self.mv_int = os.path.join(self.tempdir, 'mv_int')
    self.manifest_url = 'manifest.com'

    self.repo_mock = self.PatchObject(
        repository, 'RepoRepository', autospec=True)

    self.refresh_manifest_mock = self.PatchObject(
        manifest_version, 'RefreshManifestCheckout', autospec=True)

    self.resolve_buildspec_mock = self.PatchObject(
        manifest_version, 'ResolveBuildspec', autospec=True)

    self.resolve_version_mock = self.PatchObject(
        manifest_version, 'ResolveBuildspecVersion', autospec=True)


  def notestHelp(self):
    with self.assertRaises(SystemExit):
      repo_sync_manifest.main([
          '--help',
      ])

  def testMinimal(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest=None)
    ])

  def testMinimalExternal(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--external',
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.EXT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest=None)
    ])

  def testMinimalManifestUrl(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--manifest-url', self.manifest_url,
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.manifest_url,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest=None)
    ])

  def testBranch(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--branch', 'branch'
    ])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='branch',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest=None)
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

  def testBuildSpec(self):
    self.resolve_buildspec_mock.return_value = 'resolved_buildspec'

    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--buildspec', 'test/spec',
        '--manifest-versions-int', self.mv_int,
    ])

    # Ensure manifest-versions interactions are correct.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [
        mock.call(self.mv_int, self.MV_INT_URL)])
    self.assertEqual(self.resolve_buildspec_mock.mock_calls, [
        mock.call(self.mv_int, 'test/spec')])
    self.assertEqual(self.resolve_version_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest='resolved_buildspec')
    ])

  def testBuildSpecExternal(self):
    self.resolve_buildspec_mock.return_value = 'resolved_buildspec'

    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--buildspec', 'test/spec',
        '--manifest-versions-ext', self.mv_ext,
        '--external',
    ])

    # Ensure manifest-versions interactions are correct.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [
        mock.call(self.mv_ext, self.MV_EXT_URL)])
    self.assertEqual(self.resolve_buildspec_mock.mock_calls, [
        mock.call(self.mv_ext, 'test/spec')])
    self.assertEqual(self.resolve_version_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.EXT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest='resolved_buildspec')
    ])

  def testVersion(self):
    self.resolve_version_mock.return_value = 'resolved_buildspec'

    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--version', '1.2.3',
        '--manifest-versions-int', self.mv_int,
    ])

    # Ensure manifest-versions interactions are correct.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [
        mock.call(self.mv_int, self.MV_INT_URL)])
    self.assertEqual(self.resolve_buildspec_mock.mock_calls, [])
    self.assertEqual(self.resolve_version_mock.mock_calls, [
        mock.call(self.mv_int, '1.2.3')])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest='resolved_buildspec')
    ])

  def testVersionExternal(self):
    self.resolve_version_mock.return_value = 'resolved_buildspec'

    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--version', '1.2.3',
        '--manifest-versions-ext', self.mv_ext,
        '--external',
    ])

    # Ensure manifest-versions interactions are correct.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [
        mock.call(self.mv_ext, self.MV_EXT_URL)])
    self.assertEqual(self.resolve_buildspec_mock.mock_calls, [])
    self.assertEqual(self.resolve_version_mock.mock_calls, [
        mock.call(self.mv_ext, '1.2.3')])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.EXT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest='resolved_buildspec')
    ])

  def testBuildSpecNoManifestVersions(self):
    with self.assertRaises(AssertionError):
      repo_sync_manifest.main([
          '--repo-root', self.repo_dir,
          '--buildspec', 'test/spec',
      ])

    with self.assertRaises(AssertionError):
      repo_sync_manifest.main([
          '--repo-root', self.repo_dir,
          '--buildspec', 'test/spec',
          '--external',
      ])

  def testLocalManifest(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--manifest-file', self.manifest,
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups=None,
        ),
        mock.call().Sync(detach=True, local_manifest=self.manifest)
    ])

  def testGroups(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--manifest-file', self.manifest,
        '--groups', 'all'
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=None,
            repo_url=None,
            groups='all',
        ),
        mock.call().Sync(detach=True, local_manifest=self.manifest)
    ])

  def testOptimizations(self):
    repo_sync_manifest.main([
        '--repo-root', self.repo_dir,
        '--copy-repo', self.preload_src,
        '--git-cache', self.git_cache,
        '--repo-url', self.repo_url,
    ])

    # Ensure manifest_versions is not updated.
    self.assertEqual(self.refresh_manifest_mock.mock_calls, [])

    # Ensure RepoRepository created and Sync'd as expected.
    self.assertEqual(self.repo_mock.mock_calls, [
        mock.call(
            directory=self.repo_dir,
            manifest_repo_url=self.INT_MANIFEST_URL,
            branch='master',
            git_cache_dir=self.git_cache,
            repo_url=self.repo_url,
            groups=None,
        ),
        mock.call().PreLoad(self.preload_src),
        mock.call().Sync(detach=True, local_manifest=None)
    ])

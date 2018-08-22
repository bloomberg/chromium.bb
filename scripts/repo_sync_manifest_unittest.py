# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for scripts/repo_sync_manifest."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import repo_util
from chromite.lib import repo_util_unittest
from chromite.scripts import repo_sync_manifest


class RepoSyncManifestTest(cros_test_lib.MockTempDirTestCase):
  """Unit tests for repo_sync_manifest."""

  def setUp(self):
    self.manifest = os.path.join(self.tempdir, 'manifest.xml')
    osutils.Touch(self.manifest)

    self.repo_root = os.path.join(self.tempdir, 'root')
    os.makedirs(os.path.join(self.repo_root, '.repo'))

    self.empty_root = os.path.join(self.tempdir, 'empty')
    os.makedirs(os.path.join(self.empty_root))

    self.mock_copy = self.PatchObject(
        repo_util.Repository, 'Copy',
        side_effect=repo_util_unittest.CopySideEffects)
    self.mock_sync = self.PatchObject(repo_util.Repository, 'Sync')

  def testRepoRoot(self):
    repo_sync_manifest.main([self.manifest, '--repo-root', self.repo_root])
    self.mock_sync.assert_called_with(manifest_path=self.manifest)

  def testCopyRepo(self):
    repo_sync_manifest.main([self.manifest, '--repo-root', self.empty_root,
                             '--copy-repo', self.repo_root])
    self.mock_copy.assert_called_with(self.empty_root)
    self.mock_sync.assert_called_with(manifest_path=self.manifest)

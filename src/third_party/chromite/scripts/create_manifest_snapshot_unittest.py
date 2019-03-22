# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for scripts/repo_sync_manifest."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel_unittest
from chromite.lib import repo_util
from chromite.lib import repo_manifest
from chromite.lib import repo_manifest_unittest
from chromite.scripts import create_manifest_snapshot


MANIFEST_XML = """<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="origin"/>
  <default remote="origin"/>
  <project name="project/a" revision="f01dab1e"/>
  <project name="project/b" revision="cafe1234" upstream="short"/>
  <project name="project/c" revision="deadbeef" upstream="refs/heads/master"/>
  <project name="dupe" revision="d1" upstream="dupe"/>
  <project name="dupe" revision="d2" upstream="dupe"/>
</manifest>
"""


class CreateManifestSnapshotTest(cros_test_lib.MockTempDirTestCase,
                                 repo_manifest_unittest.XMLTestCase):
  """Unit tests for create_manifest_snapshot."""

  # pylint: disable=protected-access

  def setUp(self):
    self.manifest = repo_manifest.Manifest.FromString(MANIFEST_XML)
    self.project_a = self.manifest.GetUniqueProject('project/a')
    self.project_b = self.manifest.GetUniqueProject('project/b')
    self.project_c = self.manifest.GetUniqueProject('project/c')
    self.dupes = [x for x in self.manifest.Projects() if x.name == 'dupe']

    self.mock_is_reachable = self.PatchObject(git, 'IsReachable')
    self.mock_git_push = self.PatchObject(git, 'GitPush')

    self.repo_root = os.path.join(self.tempdir, 'root')
    os.makedirs(os.path.join(self.repo_root, '.repo'))
    self.output_file = os.path.join(self.tempdir, 'snapshot.xml')
    self.main_args = ['--repo-path', self.repo_root,
                      '--log-level', 'fatal',
                      '--jobs', '1',
                      '--output-file', self.output_file]
    self.PatchObject(repo_util.Repository, 'Manifest',
                     return_value=self.manifest)

  def testGetUpstreamBranchNoUpstream(self):
    """Test _GetUpstreamBranch with no upstream."""
    branch = create_manifest_snapshot._GetUpstreamBranch(self.project_a)
    self.assertIsNone(branch)

  def testGetUpstreamBranchShortUpstream(self):
    """Test _GetUpstreamBranch with short upstream ref."""
    branch = create_manifest_snapshot._GetUpstreamBranch(self.project_b)
    self.assertEqual(branch, 'short')

  def testGetUpstreamBranchFullUpstream(self):
    """Test _GetUpstreamBranch with full upstream ref."""
    branch = create_manifest_snapshot._GetUpstreamBranch(self.project_c)
    self.assertEqual(branch, 'master')

  def testNeedsSnapshotReachable(self):
    """Test _NeedsSnapshot with revision reachable from upstream."""
    self.mock_is_reachable.return_value = True
    result = create_manifest_snapshot._NeedsSnapshot('root', self.project_c)
    self.assertFalse(result)
    self.mock_is_reachable.assert_called_with(
        'root/project/c', 'deadbeef', 'refs/remotes/origin/master')

  def testNeedsSnapshotUnreachable(self):
    """Test _NeedsSnapshot with revision reachable from upstream."""
    self.mock_is_reachable.return_value = False
    result = create_manifest_snapshot._NeedsSnapshot('root', self.project_b)
    self.assertTrue(result)
    self.mock_is_reachable.assert_called_with(
        'root/project/b', 'cafe1234', 'refs/remotes/origin/short')

  def testNeedsSnapshotNoUpstream(self):
    """Test _NeedsSnapshot with no project upstream."""
    create_manifest_snapshot._NeedsSnapshot('root', self.project_a)
    self.mock_is_reachable.assert_called_with(
        'root/project/a', 'f01dab1e', 'refs/remotes/origin/master')

  def testNeedsSnapshotIsReachableFailure(self):
    """Test _NeedsSnapshot with no project upstream."""
    self.mock_is_reachable.side_effect = cros_build_lib.RunCommandError('', '')
    result = create_manifest_snapshot._NeedsSnapshot('root', self.project_a)
    self.assertTrue(result)

  def testMakeUniqueRefMaster(self):
    """Test _MakeUniqueRef with upstream master."""
    used = set()
    ref1 = create_manifest_snapshot._MakeUniqueRef(self.project_c, 'base', used)
    ref2 = create_manifest_snapshot._MakeUniqueRef(self.project_c, 'base', used)
    ref3 = create_manifest_snapshot._MakeUniqueRef(self.project_c, 'base', used)
    self.assertEqual(ref1, 'base')
    self.assertEqual(ref2, 'base/1')
    self.assertEqual(ref3, 'base/2')

  def testMakeUniqueRefNonMaster(self):
    """Test _MakeUniqueRef with non-master upstream."""
    used = set()
    ref1 = create_manifest_snapshot._MakeUniqueRef(self.project_b, 'base', used)
    ref2 = create_manifest_snapshot._MakeUniqueRef(self.project_b, 'base', used)
    self.assertEqual(ref1, 'base/short')
    self.assertEqual(ref2, 'base/short/1')

  def testGitPushProjectUpstream(self):
    """Test _GitPushProjectUpstream."""
    create_manifest_snapshot._GitPushProjectUpstream(
        'root', self.project_b, False)
    self.mock_git_push.assert_called_with(
        'root/project/b', 'cafe1234', git.RemoteRef('origin', 'short'),
        dry_run=False)

  def testGitPushProjectUpstreamDryRun(self):
    """Test _GitPushProjectUpstream with dry_run=True."""
    create_manifest_snapshot._GitPushProjectUpstream(
        'root', self.project_b, True)
    self.mock_git_push.assert_called_with(
        'root/project/b', 'cafe1234', git.RemoteRef('origin', 'short'),
        dry_run=True)

  def testMainNoSnapshots(self):
    """Test main with projects that don't need snapshots."""
    self.mock_is_reachable.return_value = True
    create_manifest_snapshot.main(self.main_args)
    snapshot_xml = osutils.ReadFile(self.output_file)
    self.AssertXMLAlmostEqual(snapshot_xml, MANIFEST_XML)

  def testMainSnapshots(self):
    """Test main with projects that need snapshots."""
    self.mock_is_reachable.return_value = False
    args = self.main_args + ['--snapshot-ref', 'refs/snap']
    with parallel_unittest.ParallelMock():
      create_manifest_snapshot.main(args)
    snapshot_xml = osutils.ReadFile(self.output_file)

    self.mock_git_push.assert_has_calls([
        mock.call(os.path.join(self.repo_root, 'project/a'),
                  'f01dab1e', git.RemoteRef('origin', 'refs/snap'),
                  dry_run=False),
        mock.call(os.path.join(self.repo_root, 'project/b'),
                  'cafe1234', git.RemoteRef('origin', 'refs/snap'),
                  dry_run=False),
        mock.call(os.path.join(self.repo_root, 'project/c'),
                  'deadbeef', git.RemoteRef('origin', 'refs/snap'),
                  dry_run=False),
        mock.call(os.path.join(self.repo_root, 'dupe'),
                  'd1', git.RemoteRef('origin', 'refs/snap/dupe'),
                  dry_run=False),
        mock.call(os.path.join(self.repo_root, 'dupe'),
                  'd2', git.RemoteRef('origin', 'refs/snap/dupe/1'),
                  dry_run=False),
    ], any_order=True)

    expected = repo_manifest.Manifest.FromString(MANIFEST_XML)
    for project in expected.Projects():
      if project.name == 'dupe':
        if project.revision == 'd1':
          project.upstream = 'refs/snap/dupe'
        else:
          project.upstream = 'refs/snap/dupe/1'
      else:
        project.upstream = 'refs/snap'
    expected_xml = repo_manifest_unittest.ManifestToString(expected)
    self.AssertXMLAlmostEqual(snapshot_xml, expected_xml)

  def testMainNeedsSnapshotNoSnapshotRef(self):
    """Test main with projects that need snapshots but no --snapshot-ref."""
    self.mock_is_reachable.return_value = False
    with self.assertRaises(SystemExit):
      create_manifest_snapshot.main(self.main_args)

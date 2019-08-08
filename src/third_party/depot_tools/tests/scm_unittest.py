#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for scm.py."""

import logging
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support import fake_repos
from testing_support.super_mox import SuperMoxTestBase

import scm
import subprocess2


# Access to a protected member XXX of a client class
# pylint: disable=protected-access


class BaseTestCase(SuperMoxTestBase):
  # Like unittest's assertRaises, but checks for Gclient.Error.
  def assertRaisesError(self, msg, fn, *args, **kwargs):
    try:
      fn(*args, **kwargs)
    except scm.gclient_utils.Error, e:
      self.assertEquals(e.args[0], msg)
    else:
      self.fail('%s not raised' % msg)


class BaseSCMTestCase(BaseTestCase):
  def setUp(self):
    BaseTestCase.setUp(self)
    self.mox.StubOutWithMock(scm.gclient_utils, 'CheckCallAndFilter')
    self.mox.StubOutWithMock(scm.gclient_utils, 'CheckCallAndFilterAndHeader')
    self.mox.StubOutWithMock(subprocess2, 'Popen')
    self.mox.StubOutWithMock(subprocess2, 'communicate')


class RootTestCase(BaseSCMTestCase):
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'determine_scm',
        'ElementTree',
        'gclient_utils',
        'GenFakeDiff',
        'GetCasedPath',
        'GIT',
        'glob',
        'io',
        'logging',
        'only_int',
        'os',
        'platform',
        're',
        'subprocess2',
        'sys',
        'tempfile',
        'time',
        'ValidateEmail',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm, members)


class GitWrapperTestCase(BaseSCMTestCase):
  def testMembersChanged(self):
    members = [
        'ApplyEnvVars',
        'AssertVersion',
        'Capture',
        'CaptureStatus',
        'CleanupDir',
        'current_version',
        'FetchUpstreamTuple',
        'GenerateDiff',
        'GetBranch',
        'GetBranchRef',
        'GetCheckoutRoot',
        'GetDifferentFiles',
        'GetEmail',
        'GetGitDir',
        'GetOldContents',
        'GetPatchName',
        'GetUpstreamBranch',
        'IsAncestor',
        'IsDirectoryVersioned',
        'IsInsideWorkTree',
        'IsValidRevision',
        'IsWorkTreeDirty',
        'RefToRemoteRef',
        'RemoteRefToRef',
        'ShortBranchName',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm.GIT, members)

  def testGetEmail(self):
    self.mox.StubOutWithMock(scm.GIT, 'Capture')
    scm.GIT.Capture(['config', 'user.email'], cwd=self.root_dir
                    ).AndReturn('mini@me.com')
    self.mox.ReplayAll()
    self.assertEqual(scm.GIT.GetEmail(self.root_dir), 'mini@me.com')

  def testRefToRemoteRef(self):
    remote = 'origin'
    refs = {
        'refs/branch-heads/1234': ('refs/remotes/branch-heads/', '1234'),
        # local refs for upstream branch
        'refs/remotes/%s/foobar' % remote: ('refs/remotes/%s/' % remote,
                                            'foobar'),
        '%s/foobar' % remote: ('refs/remotes/%s/' % remote, 'foobar'),
        # upstream ref for branch
        'refs/heads/foobar': ('refs/remotes/%s/' % remote, 'foobar'),
        # could be either local or upstream ref, assumed to refer to
        # upstream, but probably don't want to encourage refs like this.
        'heads/foobar': ('refs/remotes/%s/' % remote, 'foobar'),
        # underspecified, probably intended to refer to a local branch
        'foobar': None,
        # tags and other refs
        'refs/tags/TAG': None,
        'refs/changes/34/1234': None,
    }
    for k, v in refs.items():
      r = scm.GIT.RefToRemoteRef(k, remote)
      self.assertEqual(r, v, msg='%s -> %s, expected %s' % (k, r, v))

  def testRemoteRefToRef(self):
    remote = 'origin'
    refs = {
        'refs/remotes/branch-heads/1234': 'refs/branch-heads/1234',
        # local refs for upstream branch
        'refs/remotes/origin/foobar': 'refs/heads/foobar',
        # tags and other refs
        'refs/tags/TAG': 'refs/tags/TAG',
        'refs/changes/34/1234': 'refs/changes/34/1234',
        # different remote
        'refs/remotes/other-remote/foobar': None,
        # underspecified, probably intended to refer to a local branch
        'heads/foobar': None,
        'origin/foobar': None,
        'foobar': None,
        None: None,
      }
    for k, v in refs.items():
      r = scm.GIT.RemoteRefToRef(k, remote)
      self.assertEqual(r, v, msg='%s -> %s, expected %s' % (k, r, v))


class RealGitTest(fake_repos.FakeReposTestBase):
  def setUp(self):
    super(RealGitTest, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_git()
    if self.enabled:
      self.clone_dir = scm.os.path.join(self.FAKE_REPOS.git_root, 'repo_1')

  def testIsValidRevision(self):
    if not self.enabled:
      return
    # Sha1's are [0-9a-z]{32}, so starting with a 'z' or 'r' should always fail.
    self.assertFalse(scm.GIT.IsValidRevision(cwd=self.clone_dir, rev='zebra'))
    self.assertFalse(scm.GIT.IsValidRevision(cwd=self.clone_dir, rev='r123456'))
    # Valid cases
    first_rev = self.githash('repo_1', 1)
    self.assertTrue(scm.GIT.IsValidRevision(cwd=self.clone_dir, rev=first_rev))
    self.assertTrue(scm.GIT.IsValidRevision(cwd=self.clone_dir, rev='HEAD'))

  def testIsAncestor(self):
    if not self.enabled:
      return
    self.assertTrue(scm.GIT.IsAncestor(
        self.clone_dir, self.githash('repo_1', 1), self.githash('repo_1', 2)))
    self.assertFalse(scm.GIT.IsAncestor(
        self.clone_dir, self.githash('repo_1', 2), self.githash('repo_1', 1)))
    self.assertFalse(scm.GIT.IsAncestor(
        self.clone_dir, self.githash('repo_1', 1), 'zebra'))


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:

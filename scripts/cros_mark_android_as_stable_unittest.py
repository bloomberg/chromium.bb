# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_android_as_stable.py."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.scripts import cros_mark_android_as_stable


class _StubCommandResult(object):
  """Helper for mocking RunCommand results."""
  def __init__(self, msg):
    self.output = msg


class CrosMarkAndroidAsStable(cros_test_lib.MockTempDirTestCase):
  """Tests for cros_mark_android_as_stable."""

  unstable_data = 'KEYWORDS="~x86 ~arm"'
  stable_data = 'KEYWORDS="x86 arm"'

  def setUp(self):
    """Setup vars and create mock dir."""
    self.tmp_overlay = os.path.join(self.tempdir, 'chromiumos-overlay')
    self.mock_android_dir = os.path.join(self.tmp_overlay, constants.ANDROID_CP)

    ebuild = os.path.join(self.mock_android_dir,
                          constants.ANDROID_PN + '-%s.ebuild')
    self.unstable = ebuild % '9999'
    self.old_version = '25'
    self.old = ebuild % self.old_version
    self.old2_version = '50'
    self.old2 = ebuild % self.old2_version
    self.new_version = '100'
    self.new = ebuild % ('%s-r1' % self.new_version)
    self.partial_new_version = '150'
    self.not_new_version = '200'

    osutils.WriteFile(self.unstable, self.unstable_data, makedirs=True)
    osutils.WriteFile(self.old, self.stable_data, makedirs=True)
    osutils.WriteFile(self.old2, self.stable_data, makedirs=True)

    self.bucket_url = 'gs://u'
    self.build_branch = 'x'
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())

    builds = {
        'ARM': [
            self.old_version, self.old2_version, self.new_version,
            self.partial_new_version
        ],
        'X86': [self.old_version, self.old2_version, self.new_version],
    }
    for build_type, builds in builds.iteritems():
      url = self.makeTargetUrl(constants.ANDROID_BUILD_TARGETS[build_type])
      builds = '\n'.join(os.path.join(url, version) for version in builds)
      self.gs_mock.AddCmdResult(['ls', '--', url], output=builds)

    for version in [self.old_version, self.old2_version, self.new_version]:
      for target in constants.ANDROID_BUILD_TARGETS.itervalues():
        self.setupMockBuild(target, version)

    self.setupMockBuild(constants.ANDROID_BUILD_TARGETS['ARM'],
                        self.partial_new_version)
    self.setupMockBuild(constants.ANDROID_BUILD_TARGETS['X86'],
                        self.partial_new_version, False)

    for target in constants.ANDROID_BUILD_TARGETS.itervalues():
      self.setupMockBuild(target, self.not_new_version, False)

  def setupMockBuild(self, target, version, valid=True):
    """Helper to mock a build."""
    url = self.makeUrl(target, version)
    if valid:
      subdir = os.path.join(url, self.makeSubpath(target, version))
      self.gs_mock.AddCmdResult(['ls', '--', url], output=subdir)
      zipfile = os.path.join(subdir, 'file-%s.zip' % version)
      self.gs_mock.AddCmdResult(['ls', '--', subdir], output=zipfile)
    else:
      def _RaiseGSNoSuchKey(*_args, **_kwargs):
        raise gs.GSNoSuchKey('file does not exist')
      self.gs_mock.AddCmdResult(['ls', '--', url],
                                side_effect=_RaiseGSNoSuchKey)

  def makeTargetUrl(self, target):
    """Helper to return the url for a target."""
    return os.path.join(self.bucket_url,
                        '%s-%s' % (self.build_branch, target))

  def makeUrl(self, target, version):
    """Helper to return the url for a build."""
    return os.path.join(self.makeTargetUrl(target), version)

  def makeSubpath(self, target, version):
    """Helper to return the subpath for a build."""
    return '%s%s' % (target, version)

  def testIsBuildIdValid(self):
    """Test if checking if build valid."""
    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.old_version)
    self.assertTrue(subpaths)
    self.assertEquals(len(subpaths), 2)
    self.assertEquals(subpaths['ARM'], 'linux-cheets_arm-userdebug25')
    self.assertEquals(subpaths['X86'], 'linux-cheets_x86-userdebug25')

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.new_version)
    self.assertTrue(subpaths)
    self.assertEquals(len(subpaths), 2)
    self.assertEquals(subpaths['ARM'], 'linux-cheets_arm-userdebug100')
    self.assertEquals(subpaths['X86'], 'linux-cheets_x86-userdebug100')

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(
        self.bucket_url, self.build_branch, self.partial_new_version)
    self.assertEqual(subpaths, None)

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.not_new_version)
    self.assertEqual(subpaths, None)

  def testFindAndroidCandidates(self):
    """Test creation of stable ebuilds from mock dir."""
    (unstable, stable) = cros_mark_android_as_stable.FindAndroidCandidates(
        self.mock_android_dir)

    stable_ebuild_paths = [x.ebuild_path for x in stable]
    self.assertEqual(unstable.ebuild_path, self.unstable)
    self.assertEqual(len(stable), 2)
    self.assertIn(self.old, stable_ebuild_paths)
    self.assertIn(self.old2, stable_ebuild_paths)

  def testGetLatestBuild(self):
    """Test determination of latest build from gs bucket."""
    version, subpaths = cros_mark_android_as_stable.GetLatestBuild(
        self.bucket_url, self.build_branch)
    self.assertEqual(version, self.new_version)
    self.assertTrue(subpaths)
    self.assertEquals(len(subpaths), 2)
    self.assertEquals(subpaths['ARM'], 'linux-cheets_arm-userdebug100')
    self.assertEquals(subpaths['X86'], 'linux-cheets_x86-userdebug100')

  def testGetAndroidRevisionListLink(self):
    """Test generation of revision diff list."""
    old_ebuild = portage_util.EBuild(self.old)
    old2_ebuild = portage_util.EBuild(self.old2)
    link = cros_mark_android_as_stable.GetAndroidRevisionListLink(
        self.build_branch, old_ebuild, old2_ebuild)
    self.assertEqual(link, ('http://android-build-uber.corp.google.com/'
                            'repo.html?last_bid=25&bid=50&branch=x'))

  def testMarkAndroidEBuildAsStable(self):
    """Test updating of ebuild."""
    self.PatchObject(cros_build_lib, 'RunCommand')
    git_mock = self.PatchObject(git, 'RunGit')
    commit_mock = self.PatchObject(portage_util.EBuild, 'CommitChange')
    stable_candidate = portage_util.EBuild(self.old2)
    unstable = portage_util.EBuild(self.unstable)
    android_version = self.new_version
    package_dir = self.mock_android_dir
    subpaths_dict = {
        'ARM': 'linux-cheets_arm-userdebug100',
        'X86': 'linux-cheets_x86-userdebug100',
    }
    version_atom = cros_mark_android_as_stable.MarkAndroidEBuildAsStable(
        stable_candidate, unstable, constants.ANDROID_PN,
        android_version, subpaths_dict, package_dir,
        self.build_branch)
    git_mock.assert_has_calls([
        mock.call(package_dir, ['add', self.new]),
        mock.call(package_dir, ['add', 'Manifest']),
    ])
    commit_mock.assert_call(mock.call('latest', package_dir))
    self.assertEqual(version_atom,
                     '%s-%s-r1' % (constants.ANDROID_CP, self.new_version))

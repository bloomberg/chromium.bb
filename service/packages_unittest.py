# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages service tests."""

from __future__ import print_function

import re

from chromite.cbuildbot import manifest_version
from chromite.lib import build_target_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib.chroot_lib import Chroot
from chromite.service import packages


class UprevAndroidTest(cros_test_lib.RunCommandTestCase):
  """Uprev android tests."""

  def test_success(self):
    """Test successful run handling."""
    self.PatchObject(packages, '_parse_android_atom',
                     return_value='ANDROID_ATOM=android/android-1.0')

    build_targets = [build_target_util.BuildTarget(t) for t in ['foo', 'bar']]

    packages.uprev_android('refs/tracking-branch', 'android/package',
                           'refs/android-build-branch', Chroot(),
                           build_targets=build_targets)
    self.assertCommandContains(['cros_mark_android_as_stable',
                                '--boards=foo:bar'])
    self.assertCommandContains(['emerge-foo'])
    self.assertCommandContains(['emerge-bar'])

  def test_no_uprev(self):
    """Test no uprev handling."""
    self.PatchObject(packages, '_parse_android_atom', return_value=None)
    build_targets = [build_target_util.BuildTarget(t) for t in ['foo', 'bar']]
    packages.uprev_android('refs/tracking-branch', 'android/package',
                           'refs/android-build-branch', Chroot(),
                           build_targets=build_targets)

    self.assertCommandContains(['cros_mark_android_as_stable',
                                '--boards=foo:bar'])
    self.assertCommandContains(['emerge-foo'], expected=False)
    self.assertCommandContains(['emerge-bar'], expected=False)


class UprevBuildTargetsTest(cros_test_lib.RunCommandTestCase):
  """uprev_build_targets tests."""

  def test_invalid_type_fails(self):
    """Test invalid type fails."""
    with self.assertRaises(AssertionError):
      packages.uprev_build_targets([build_target_util.BuildTarget('foo')],
                                   'invalid')

  def test_none_type_fails(self):
    """Test None type fails."""
    with self.assertRaises(AssertionError):
      packages.uprev_build_targets([build_target_util.BuildTarget('foo')],
                                   None)


class UprevsVersionedPackageTest(cros_test_lib.MockTestCase):
  """uprevs_versioned_package decorator test."""

  @packages.uprevs_versioned_package('category/package')
  def uprev_category_package(self, *args, **kwargs):
    """Registered function for testing."""

  def test_calls_function(self):
    """Test calling a registered function."""
    patch = self.PatchObject(self, 'uprev_category_package')

    cpv = portage_util.SplitCPV('category/package', strict=False)
    packages.uprev_versioned_package(cpv, [], [], Chroot())

    patch.assert_called()

  def test_unregistered_package(self):
    """Test calling with an unregistered package."""
    cpv = portage_util.SplitCPV('does-not/exist', strict=False)

    with self.assertRaises(packages.UnknownPackageError):
      packages.uprev_versioned_package(cpv, [], [], Chroot())


class GetBestVisibleTest(cros_test_lib.TestCase):
  """get_best_visible tests."""

  def test_empty_atom_fails(self):
    with self.assertRaises(AssertionError):
      packages.get_best_visible('')


class HasPrebuiltTest(cros_test_lib.TestCase):
  """has_prebuilt tests."""

  def test_empty_atom_fails(self):
    with self.assertRaises(AssertionError):
      packages.has_prebuilt('')


class AndroidVersionsTest(cros_test_lib.MockTestCase):
  """Tests getting android versions."""

  def setUp(self):
    package_result = [
        'chromeos-base/android-container-nyc-4717008-r1',
        'chromeos-base/update_engine-0.0.3-r3408']
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     return_value=package_result)
    self.board = 'board'
    self.PatchObject(portage_util, 'FindEbuildForBoardPackage',
                     return_value='chromeos-base/android-container-nyc')
    FakeEnvironment = {
        'ARM_TARGET': '3-linux-target'
    }
    self.PatchObject(osutils, 'SourceEnvironment',
                     return_value=FakeEnvironment)

  def test_determine_android_version(self):
    """Tests that a valid android version is returned."""
    version = packages.determine_android_version(self.board)
    self.assertEqual(version, '4717008')

  def test_determine_android_version_when_not_present(self):
    """Tests that a None is returned for version when android is not present."""
    package_result = ['chromeos-base/update_engine-0.0.3-r3408']
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     return_value=package_result)
    version = packages.determine_android_version(self.board)
    self.assertEqual(version, None)

  def test_determine_android_branch(self):
    """Tests that a valid android branch is returned."""
    branch = packages.determine_android_branch(self.board)
    self.assertEqual(branch, '3')

  def test_determine_android_branch_when_not_present(self):
    """Tests that a None is returned for branch when android is not present."""
    package_result = ['chromeos-base/update_engine-0.0.3-r3408']
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     return_value=package_result)
    branch = packages.determine_android_branch(self.board)
    self.assertEqual(branch, None)

  def test_determine_android_target(self):
    """Tests that a valid android target is returned."""
    target = packages.determine_android_target(self.board)
    self.assertEqual(target, 'cheets')

  def test_determine_android_target_when_not_present(self):
    """Tests that a None is returned for target when android is not present."""
    package_result = ['chromeos-base/update_engine-0.0.3-r3408']
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     return_value=package_result)
    target = packages.determine_android_target(self.board)
    self.assertEqual(target, None)

  def test_determine_android_version_handle_exception(self):
    """Tests handling RunCommandError inside determine_android_version."""
    # Mock what happens when portage returns that bubbles up (via RunCommand)
    # inside portage_util.GetPackageDependencies.
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     side_effect=cros_build_lib.RunCommandError('error'))
    target = packages.determine_android_version(self.board)
    self.assertEqual(target, None)

  def test_determine_android_package_handle_exception(self):
    """Tests handling RunCommandError inside determine_android_package."""
    # Mock what happens when portage returns that bubbles up (via RunCommand)
    # inside portage_util.GetPackageDependencies.
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     side_effect=cros_build_lib.RunCommandError('error'))
    target = packages.determine_android_package(self.board)
    self.assertEqual(target, None)


class ChromeVersionsTest(cros_test_lib.MockTestCase):
  """Tests getting chrome version."""

  def setUp(self):
    self.build_target = build_target_util.BuildTarget('board')

  def test_determine_chrome_version(self):
    """Tests that a valid chrome version is returned."""
    # Mock PortageqBestVisible to return a valid chrome version string.
    r1_cpf = 'chromeos-base/chromeos-chrome-78.0.3900.0_rc-r1'
    r1_cpv = portage_util.SplitCPV(r1_cpf)
    self.PatchObject(portage_util, 'PortageqBestVisible',
                     return_value=r1_cpv)

    chrome_version = packages.determine_chrome_version(self.build_target)
    version_numbers = chrome_version.split('.')
    self.assertEqual(len(version_numbers), 4)
    self.assertEqual(int(version_numbers[0]), 78)


class PlatformVersionsTest(cros_test_lib.MockTestCase):
  """Tests getting platform version."""

  def test_determine_platform_version(self):
    """Test checking that a valid platform version is returned."""
    platform_version = packages.determine_platform_version()
    # The returned platform version is something like 12603.0.0.
    version_string_list = platform_version.split('.')
    self.assertEqual(len(version_string_list), 3)
    # We don't want to check an exact version, but the first number should be
    # non-zero.
    self.assertGreaterEqual(int(version_string_list[0]), 1)

  def test_determine_milestone_version(self):
    """Test checking that a valid milestone version is returned."""
    milestone_version = packages.determine_milestone_version()
    # Milestone version should be non-zero
    self.assertGreaterEqual(int(milestone_version), 1)

  def test_determine_full_version(self):
    """Test checking that a valid full version is returned."""
    full_version = packages.determine_full_version()
    pattern = r'^R(\d+)-(\d+.\d+.\d+(-rc\d+)*)'
    m = re.match(pattern, full_version)
    self.assertTrue(m)
    milestone_version = m.group(1)
    self.assertGreaterEqual(int(milestone_version), 1)

  def test_versions_based_on_mock(self):
    # Create a test version_info object, and than mock VersionInfo.from_repo
    # return it.
    test_platform_version = '12575.0.0'
    test_chrome_branch = '75'
    version_info_mock = manifest_version.VersionInfo(test_platform_version)
    version_info_mock.chrome_branch = test_chrome_branch
    self.PatchObject(manifest_version.VersionInfo, 'from_repo',
                     return_value=version_info_mock)
    test_full_version = 'R' + test_chrome_branch + '-' + test_platform_version
    platform_version = packages.determine_platform_version()
    milestone_version = packages.determine_milestone_version()
    full_version = packages.determine_full_version()
    self.assertEqual(platform_version, test_platform_version)
    self.assertEqual(milestone_version, test_chrome_branch)
    self.assertEqual(full_version, test_full_version)

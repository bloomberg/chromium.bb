# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages service tests."""

from __future__ import print_function

import json
import os
import re

from google.protobuf import json_format
from google.protobuf.field_mask_pb2 import FieldMask

from chromite.api.gen.config.replication_config_pb2 import (
    ReplicationConfig, FileReplicationRule, FILE_TYPE_JSON,
    REPLICATION_TYPE_FILTER
)
from chromite.cbuildbot import manifest_version
from chromite.lib import build_target_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib.chroot_lib import Chroot
from chromite.lib.uprev_lib import GitRef
from chromite.service import packages

D = cros_test_lib.Directory


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


class ReplicatePrivateConfigTest(cros_test_lib.MockTempDirTestCase):
  """replicate_private_config tests."""

  def setUp(self):
    # Set up fake public and private chromeos-config overlays.
    private_package_root = (
        'overlay-coral-private/chromeos-base/chromeos-config-bsp-coral-private')
    self.public_package_root = (
        'overlay-coral/chromeos-base/chromeos-config-bsp-coral')
    file_layout = (
        D(os.path.join(private_package_root, 'files'), ['build_config.json']),
        D(private_package_root, ['replication_config.jsonpb']),
        D(
            os.path.join(self.public_package_root, 'files'),
            ['build_config.json']),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    # Private config contains 'a' and 'b' fields.
    private_config_path = os.path.join(self.tempdir, private_package_root,
                                       'files', 'build_config.json')
    osutils.WriteFile(private_config_path,
                      json.dumps({'chromeos': {
                          'configs': [{
                              'a': 3,
                              'b': 2
                          }]
                      }}))

    # Public config only contains the 'a' field. Note that the value of 'a' is
    # 1 in the public config; it will get updated to 3 when the private config
    # is replicated.
    self.public_config_path = os.path.join(self.tempdir,
                                           self.public_package_root, 'files',
                                           'build_config.json')
    osutils.WriteFile(self.public_config_path,
                      json.dumps({'chromeos': {
                          'configs': [{
                              'a': 1
                          }]
                      }}))

    # Put a ReplicationConfig JSONPB in the private package. Note that it
    # specifies only the 'a' field is replicated.
    self.replication_config_path = os.path.join(self.tempdir,
                                                private_package_root,
                                                'replication_config.jsonpb')
    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=private_config_path,
            destination_path=self.public_config_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a']))
    ])

    osutils.WriteFile(self.replication_config_path,
                      json_format.MessageToJson(replication_config))
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)

  def test_replicate_private_config(self):
    """Basic replication test."""
    refs = [GitRef(path='overlay-coral-private', ref='master', revision='123')]
    result = packages.replicate_private_config(
        _build_targets=None, refs=refs, _chroot=None)

    self.assertEqual(len(result.modified), 1)
    # The public build_config.json was modified.
    self.assertEqual(result.modified[0].files, [self.public_config_path])
    self.assertEqual(result.modified[0].new_version, '123')

    # The update from the private build_config.json was copied to the public.
    # Note that only the 'a' field is present, as per destination_fields.
    with open(self.public_config_path, 'r') as f:
      self.assertEqual(json.load(f), {'chromeos': {'configs': [{'a': 3}]}})

  def test_replicate_private_config_wrong_number_of_refs(self):
    """An error is thrown if there is not exactly one ref."""
    with self.assertRaisesRegex(ValueError, 'Expected exactly one ref'):
      packages.replicate_private_config(
          _build_targets=None, refs=[], _chroot=None)

    with self.assertRaisesRegex(ValueError, 'Expected exactly one ref'):
      refs = [
          GitRef(path='a', ref='master', revision='1'),
          GitRef(path='a', ref='master', revision='2')
      ]
      packages.replicate_private_config(
          _build_targets=None, refs=refs, _chroot=None)

  def test_replicate_private_config_replication_config_missing(self):
    """An error is thrown if there is not a replication config."""
    os.remove(self.replication_config_path)
    with self.assertRaisesRegex(
        ValueError, 'Expected ReplicationConfig missing at %s' %
        self.replication_config_path):
      refs = [
          GitRef(path='overlay-coral-private', ref='master', revision='123')
      ]
      packages.replicate_private_config(
          _build_targets=None, refs=refs, _chroot=None)


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

  def test_determine_android_package_callers_handle_exception(self):
    """Tests handling RunCommandError by determine_android_package callers."""
    # Mock what happens when portage returns that bubbles up (via RunCommand)
    # inside portage_util.GetPackageDependencies.
    self.PatchObject(portage_util, 'GetPackageDependencies',
                     side_effect=cros_build_lib.RunCommandError('error'))
    # Verify that target is None, as expected.
    target = packages.determine_android_package(self.board)
    self.assertEqual(target, None)
    # determine_android_branch calls determine_android_package
    branch = packages.determine_android_branch(self.board)
    self.assertEqual(branch, None)
    # determine_android_target calls determine_android_package
    target = packages.determine_android_target(self.board)
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

  def test_determine_chrome_version_handle_exception(self):
    # Mock what happens when portage throws an exception that bubbles up (via
    # RunCommand)inside portage_util.PortageqBestVisible.
    self.PatchObject(portage_util, 'PortageqBestVisible',
                     side_effect=cros_build_lib.RunCommandError('error'))
    target = packages.determine_chrome_version(self.build_target)
    self.assertEqual(target, None)


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

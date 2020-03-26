# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages service tests."""

from __future__ import print_function

import json
import os
import re
import sys

from google.protobuf import json_format
from google.protobuf.field_mask_pb2 import FieldMask

from chromite.api.gen.config.replication_config_pb2 import (
    ReplicationConfig, FileReplicationRule, FILE_TYPE_JSON,
    REPLICATION_TYPE_FILTER
)
from chromite.cbuildbot import manifest_version
from chromite.lib import build_target_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import portage_util
from chromite.lib.chroot_lib import Chroot
from chromite.lib.uprev_lib import GitRef
from chromite.service import packages


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


D = cros_test_lib.Directory


class UprevAndroidTest(cros_test_lib.RunCommandTestCase):
  """Uprev android tests."""

  def test_success(self):
    """Test successful run handling."""
    self.rc.AddCmdResult(partial_mock.In('cros_mark_android_as_stable'),
                         stdout='ANDROID_ATOM=android/android-1.0\n')
    build_targets = [build_target_lib.BuildTarget(t) for t in ['foo', 'bar']]

    packages.uprev_android('refs/tracking-branch', 'android/package',
                           'refs/android-build-branch', Chroot(),
                           build_targets=build_targets)
    self.assertCommandContains(['cros_mark_android_as_stable',
                                '--boards=foo:bar'])
    self.assertCommandContains(['emerge-foo'])
    self.assertCommandContains(['emerge-bar'])

  def test_no_uprev(self):
    """Test no uprev handling."""
    self.rc.AddCmdResult(partial_mock.In('cros_mark_android_as_stable'),
                         stdout='')
    build_targets = [build_target_lib.BuildTarget(t) for t in ['foo', 'bar']]
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
      packages.uprev_build_targets([build_target_lib.BuildTarget('foo')],
                                   'invalid')

  def test_none_type_fails(self):
    """Test None type fails."""
    with self.assertRaises(AssertionError):
      packages.uprev_build_targets([build_target_lib.BuildTarget('foo')],
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

    # TODO(crbug/1065172): Invalid assertion that had previously been mocked.
    # patch.assert_called()

  def test_unregistered_package(self):
    """Test calling with an unregistered package."""
    cpv = portage_util.SplitCPV('does-not/exist', strict=False)

    with self.assertRaises(packages.UnknownPackageError):
      packages.uprev_versioned_package(cpv, [], [], Chroot())


class UprevEbuildFromPinTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests uprev_ebuild_from_pin function"""

  package = 'category/package'
  version = '1.2.3'
  new_version = '1.2.4'
  ebuild_template = 'package-%s-r1.ebuild'
  ebuild = ebuild_template % version
  version_pin = 'VERSION-PIN'
  manifest = 'Manifest'

  def test_uprev_ebuild(self):
    """Tests uprev of ebuild with version path"""
    file_layout = (
        D(self.package, [self.ebuild, self.version_pin, self.manifest]),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    package_path = os.path.join(self.tempdir, self.package)
    version_pin_path = os.path.join(package_path, self.version_pin)
    self.WriteTempFile(version_pin_path, self.new_version)

    result = packages.uprev_ebuild_from_pin(package_path, version_pin_path,
                                            chroot=Chroot())
    self.assertEqual(len(result.modified), 1,
                     'unexpected number of results: %s' % len(result.modified))

    mod = result.modified[0]
    self.assertEqual(mod.new_version, self.new_version,
                     'unexpected version number: %s' % mod.new_version)

    old_ebuild_path = os.path.join(package_path,
                                   self.ebuild_template % self.version)
    new_ebuild_path = os.path.join(package_path,
                                   self.ebuild_template % self.new_version)
    manifest_path = os.path.join(package_path, 'Manifest')

    expected_modified_files = [old_ebuild_path, new_ebuild_path, manifest_path]
    self.assertCountEqual(mod.files, expected_modified_files)

    self.assertCommandContains(['ebuild', 'manifest'])

  def test_no_ebuild(self):
    """Tests assertion is raised if package has no ebuilds"""
    file_layout = (
        D(self.package, [self.version_pin, self.manifest]),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    package_path = os.path.join(self.tempdir, self.package)
    version_pin_path = os.path.join(package_path, self.version_pin)
    self.WriteTempFile(version_pin_path, self.new_version)

    with self.assertRaises(packages.UprevError):
      packages.uprev_ebuild_from_pin(package_path, version_pin_path,
                                     chroot=Chroot())

  def test_multiple_ebuilds(self):
    """Tests assertion is raised if multiple ebuilds are present for package"""
    file_layout = (
        D(self.package, [self.version_pin, self.ebuild,
                         self.ebuild_template % '1.2.1',
                         self.manifest]),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    package_path = os.path.join(self.tempdir, self.package)
    version_pin_path = os.path.join(package_path, self.version_pin)
    self.WriteTempFile(version_pin_path, self.new_version)

    with self.assertRaises(packages.UprevError):
      packages.uprev_ebuild_from_pin(package_path, version_pin_path,
                                     chroot=Chroot())


class ReplicatePrivateConfigTest(cros_test_lib.RunCommandTempDirTestCase):
  """replicate_private_config tests."""

  def setUp(self):
    # Set up fake public and private chromeos-config overlays.
    private_package_root = (
        'src/private-overlays/overlay-coral-private/chromeos-base/'
        'chromeos-config-bsp'
    )
    self.public_package_root = (
        'src/overlays/overlay-coral/chromeos-base/chromeos-config-bsp')
    file_layout = (
        D(os.path.join(private_package_root, 'files'), ['build_config.json']),
        D(private_package_root, ['replication_config.jsonpb']),
        D(
            os.path.join(self.public_package_root, 'files'),
            ['build_config.json']),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    # Private config contains 'a' and 'b' fields.
    self.private_config_path = os.path.join(private_package_root, 'files',
                                            'build_config.json')
    self.WriteTempFile(
        self.private_config_path,
        json.dumps({'chromeos': {
            'configs': [{
                'a': 3,
                'b': 2
            }]
        }}))

    # Public config only contains the 'a' field. Note that the value of 'a' is
    # 1 in the public config; it will get updated to 3 when the private config
    # is replicated.
    self.public_config_path = os.path.join(self.public_package_root, 'files',
                                           'build_config.json')
    self.WriteTempFile(self.public_config_path,
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
            source_path=self.private_config_path,
            destination_path=self.public_config_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a']))
    ])

    osutils.WriteFile(self.replication_config_path,
                      json_format.MessageToJson(replication_config))
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)

    self.rc.SetDefaultCmdResult(side_effect=self._write_generated_c_files)

  def _write_generated_c_files(self, *_args, **_kwargs):
    """Write fake generated C files to the public output dir.

    Note that this function accepts args and kwargs so it can be used as a side
    effect.
    """
    output_dir = os.path.join(self.public_package_root, 'files')
    self.WriteTempFile(os.path.join(output_dir, 'config.c'), '')
    self.WriteTempFile(os.path.join(output_dir, 'ec_config.c'), '')
    self.WriteTempFile(os.path.join(output_dir, 'ec_config.h'), '')

  def _write_incorrect_generated_c_files(self, *_args, **_kwargs):
    """Similar to _write_generated_c_files, with an expected file missing.

    Note that this function accepts args and kwargs so it can be used as a side
    effect.
    """
    output_dir = os.path.join(self.public_package_root, 'files')
    self.WriteTempFile(os.path.join(output_dir, 'config.c'), '')
    self.WriteTempFile(os.path.join(output_dir, 'ec_config.c'), '')

  def test_replicate_private_config(self):
    """Basic replication test."""
    refs = [
        GitRef(
            path='/chromeos/overlays/overlay-coral-private',
            ref='master',
            revision='123')
    ]
    chroot = Chroot()
    result = packages.replicate_private_config(
        _build_targets=None, refs=refs, chroot=chroot)

    self.assertCommandContains([
        'cros_config_schema', '-m',
        os.path.join(constants.CHROOT_SOURCE_ROOT, self.public_config_path),
        '-g',
        os.path.join(constants.CHROOT_SOURCE_ROOT, self.public_package_root,
                     'files'), '-f', '"TRUE"'
    ],
                               enter_chroot=True,
                               chroot_args=chroot.get_enter_args())

    self.assertEqual(len(result.modified), 1)
    # The public build_config.json and generated C files were modified.
    expected_modified_files = [
        os.path.join(self.tempdir, self.public_config_path),
        os.path.join(self.tempdir, self.public_package_root, 'files',
                     'config.c'),
        os.path.join(self.tempdir, self.public_package_root, 'files',
                     'ec_config.c'),
        os.path.join(self.tempdir, self.public_package_root, 'files',
                     'ec_config.h'),
    ]
    self.assertEqual(result.modified[0].files, expected_modified_files)
    self.assertEqual(result.modified[0].new_version, '123')

    # The update from the private build_config.json was copied to the public.
    # Note that only the 'a' field is present, as per destination_fields.
    self.assertEqual(
        json.loads(self.ReadTempFile(self.public_config_path)),
        {'chromeos': {
            'configs': [{
                'a': 3
            }]
        }})

  def test_replicate_private_config_no_build_config(self):
    """If there is no build config, don't generate C files."""
    # Modify the replication config to write to "other_config.json" instead of
    # "build_config.json"
    modified_destination_path = self.public_config_path.replace(
        'build_config', 'other_config')
    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.private_config_path,
            destination_path=modified_destination_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a']))
    ])
    osutils.WriteFile(self.replication_config_path,
                      json_format.MessageToJson(replication_config))

    refs = [
        GitRef(
            path='/chromeos/overlays/overlay-coral-private',
            ref='master',
            revision='123')
    ]
    result = packages.replicate_private_config(
        _build_targets=None, refs=refs, chroot=Chroot())

    self.assertEqual(len(result.modified), 1)
    self.assertEqual(result.modified[0].files,
                     [os.path.join(self.tempdir, modified_destination_path)])

  def test_replicate_private_config_multiple_build_configs(self):
    """An error is thrown if there is more than one build config."""
    replication_config = ReplicationConfig(file_replication_rules=[
        FileReplicationRule(
            source_path=self.private_config_path,
            destination_path=self.public_config_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a'])),
        FileReplicationRule(
            source_path=self.private_config_path,
            destination_path=self.public_config_path,
            file_type=FILE_TYPE_JSON,
            replication_type=REPLICATION_TYPE_FILTER,
            destination_fields=FieldMask(paths=['a']))
    ])

    osutils.WriteFile(self.replication_config_path,
                      json_format.MessageToJson(replication_config))

    refs = [
        GitRef(
            path='/chromeos/overlays/overlay-coral-private',
            ref='master',
            revision='123')
    ]
    with self.assertRaisesRegex(
        ValueError, 'Expected at most one build_config.json destination path.'):
      packages.replicate_private_config(
          _build_targets=None, refs=refs, chroot=Chroot())

  def test_replicate_private_config_generated_files_incorrect(self):
    """An error is thrown if generated C files are missing."""
    self.rc.SetDefaultCmdResult(
        side_effect=self._write_incorrect_generated_c_files)

    refs = [
        GitRef(
            path='/chromeos/overlays/overlay-coral-private',
            ref='master',
            revision='123')
    ]
    chroot = Chroot()

    with self.assertRaisesRegex(packages.GeneratedCrosConfigFilesError,
                                'Expected to find generated C files'):
      packages.replicate_private_config(
          _build_targets=None, refs=refs, chroot=chroot)

  def test_replicate_private_config_wrong_number_of_refs(self):
    """An error is thrown if there is not exactly one ref."""
    with self.assertRaisesRegex(ValueError, 'Expected exactly one ref'):
      packages.replicate_private_config(
          _build_targets=None, refs=[], chroot=None)

    with self.assertRaisesRegex(ValueError, 'Expected exactly one ref'):
      refs = [
          GitRef(path='a', ref='master', revision='1'),
          GitRef(path='a', ref='master', revision='2')
      ]
      packages.replicate_private_config(
          _build_targets=None, refs=refs, chroot=None)

  def test_replicate_private_config_replication_config_missing(self):
    """An error is thrown if there is not a replication config."""
    os.remove(self.replication_config_path)
    with self.assertRaisesRegex(
        ValueError, 'Expected ReplicationConfig missing at %s' %
        self.replication_config_path):
      refs = [
          GitRef(
              path='/chromeos/overlays/overlay-coral-private',
              ref='master',
              revision='123')
      ]
      packages.replicate_private_config(
          _build_targets=None, refs=refs, chroot=None)

  def test_replicate_private_config_wrong_git_ref_path(self):
    """An error is thrown if the git ref doesn't point to a private overlay."""
    with self.assertRaisesRegex(ValueError, 'ref.path must match the pattern'):
      refs = [
          GitRef(
              path='a/b/c',
              ref='master',
              revision='123')
      ]
      packages.replicate_private_config(
          _build_targets=None, refs=refs, chroot=None)


class GetBestVisibleTest(cros_test_lib.TestCase):
  """get_best_visible tests."""

  def test_empty_atom_fails(self):
    with self.assertRaises(AssertionError):
      packages.get_best_visible('')


class HasPrebuiltTest(cros_test_lib.MockTestCase):
  """has_prebuilt tests."""

  def test_empty_atom_fails(self):
    """Test an empty atom results in an error."""
    with self.assertRaises(AssertionError):
      packages.has_prebuilt('')

  def test_use_flags(self):
    """Test use flags get propagated correctly."""
    # We don't really care about the result, just the env handling.
    patch = self.PatchObject(portage_util, 'HasPrebuilt', return_value=True)

    packages.has_prebuilt('cat/pkg-1.2.3', useflags='useflag')
    patch.assert_called_with('cat/pkg-1.2.3', board=None,
                             extra_env={'USE': 'useflag'})

  def test_env_use_flags(self):
    """Test env use flags get propagated correctly with passed useflags."""
    # We don't really care about the result, just the env handling.
    patch = self.PatchObject(portage_util, 'HasPrebuilt', return_value=True)
    # Add some flags to the environment.
    existing_flags = 'already set flags'
    self.PatchObject(os.environ, 'get', return_value=existing_flags)

    new_flags = 'useflag'
    packages.has_prebuilt('cat/pkg-1.2.3', useflags=new_flags)
    expected = '%s %s' % (existing_flags, new_flags)
    patch.assert_called_with('cat/pkg-1.2.3', board=None,
                             extra_env={'USE': expected})



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
    self.build_target = build_target_lib.BuildTarget('board')

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

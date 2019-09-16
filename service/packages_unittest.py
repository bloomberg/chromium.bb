# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages service tests."""

from __future__ import print_function

from chromite.lib import build_target_util
from chromite.lib import cros_test_lib
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

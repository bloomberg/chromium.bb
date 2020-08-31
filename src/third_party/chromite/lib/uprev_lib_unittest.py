# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""uprev_lib tests."""

from __future__ import division
from __future__ import print_function

import os
import sys

import mock

import chromite as cr
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import uprev_lib
from chromite.lib.build_target_lib import BuildTarget
from chromite.lib.chroot_lib import Chroot

assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class ChromeVersionTest(cros_test_lib.TestCase):
  """Tests for best_chrome_version and get_chrome_version_from_refs."""

  def setUp(self):
    # The tag ref template.
    ref_tpl = 'refs/tags/%s'

    self.best = '4.3.2.1'
    self.versions = ['1.2.3.4', self.best, '4.2.2.1', '4.3.1.4']
    self.best_ref = uprev_lib.GitRef('/path', ref_tpl % self.best, 'abc123')
    self.refs = [uprev_lib.GitRef('/path', ref_tpl % v, 'abc123')
                 for v in self.versions]

    self.unstable = '9999'
    self.unstable_versions = self.versions + [self.unstable]

  def test_single_version(self):
    """Test a single version."""
    self.assertEqual(self.best, uprev_lib.best_chrome_version([self.best]))

  def test_multiple_versions(self):
    """Test a single version."""
    self.assertEqual(self.best, uprev_lib.best_chrome_version(self.versions))

  def test_no_versions_fail(self):
    """Test no versions given."""
    with self.assertRaises(AssertionError):
      uprev_lib.best_chrome_version([])

  def test_unstable_only(self):
    """Test the unstable version."""
    self.assertEqual(self.unstable,
                     uprev_lib.best_chrome_version([self.unstable]))

  def test_unstable_multiple(self):
    """Test unstable alongside multiple other versions."""
    self.assertEqual(self.unstable,
                     uprev_lib.best_chrome_version(self.unstable_versions))

  def test_single_ref(self):
    """Test a single ref."""
    self.assertEqual(self.best,
                     uprev_lib.get_chrome_version_from_refs([self.best_ref]))

  def test_multiple_refs(self):
    """Test multiple refs."""
    self.assertEqual(self.best,
                     uprev_lib.get_chrome_version_from_refs(self.refs))

  def test_no_refs_fail(self):
    """Test no versions given."""
    with self.assertRaises(AssertionError):
      uprev_lib.get_chrome_version_from_refs([])


class BestChromeEbuildTests(cros_test_lib.TempDirTestCase):
  """Test for best_chrome_ebuild."""

  def setUp(self):
    # Setup some ebuilds to test against.
    ebuild = os.path.join(self.tempdir, 'chromeos-chrome-%s-r12.ebuild')
    # The best version we'll be expecting.
    best_version = '4.3.2.1'
    best_ebuild_path = ebuild % best_version

    # Other versions to set up to compare against.
    versions = ['1.2.3.4', '4.3.2.0']
    ebuild_paths = [ebuild % version for version in versions]

    # Create a separate ebuild with the same chrome version as the best ebuild.
    rev_tiebreak_ebuild_path = best_ebuild_path.replace('-r12', '-r2')
    ebuild_paths.append(rev_tiebreak_ebuild_path)

    # Write stable ebuild data.
    stable_data = 'KEYWORDS=*'
    osutils.WriteFile(best_ebuild_path, stable_data)
    for path in ebuild_paths:
      osutils.WriteFile(path, stable_data)

    # Create the ebuilds.
    ebuilds = [uprev_lib.ChromeEBuild(path) for path in ebuild_paths]
    self.best_ebuild = uprev_lib.ChromeEBuild(best_ebuild_path)
    self.ebuilds = ebuilds + [self.best_ebuild]

  def test_no_ebuilds(self):
    """Test error on no ebuilds provided."""
    with self.assertRaises(AssertionError):
      uprev_lib.best_chrome_ebuild([])

  def test_single_ebuild(self):
    """Test a single ebuild."""
    best = uprev_lib.best_chrome_ebuild([self.best_ebuild])
    self.assertEqual(self.best_ebuild.ebuild_path, best.ebuild_path)

  def test_multiple_ebuilds(self):
    """Test multiple ebuilds."""
    best = uprev_lib.best_chrome_ebuild(self.ebuilds)
    self.assertEqual(self.best_ebuild.ebuild_path, best.ebuild_path)


class FindChromeEbuildsTest(cros_test_lib.TempDirTestCase):
  """find_chrome_ebuild tests."""

  def setUp(self):
    ebuild = os.path.join(self.tempdir, 'chromeos-chrome-%s.ebuild')
    self.unstable = ebuild % '9999'
    self.alpha_unstable = ebuild % '4.3.2.1_alpha-r12'
    self.best_stable = ebuild % '4.3.2.1_rc-r12'
    self.old_stable = ebuild % '4.3.2.1_rc-r2'

    unstable_data = 'KEYWORDS=~*'
    stable_data = 'KEYWORDS=*'

    osutils.WriteFile(self.unstable, unstable_data)
    osutils.WriteFile(self.alpha_unstable, unstable_data)
    osutils.WriteFile(self.best_stable, stable_data)
    osutils.WriteFile(self.old_stable, stable_data)

  def test_find_all(self):
    unstable, stables = uprev_lib.find_chrome_ebuilds(self.tempdir)
    self.assertEqual(self.unstable, unstable.ebuild_path)
    self.assertCountEqual([self.best_stable, self.old_stable],
                          [stable.ebuild_path for stable in stables])


class UprevChromeManagerTest(cros_test_lib.MockTempDirTestCase):
  """UprevChromeManager tests."""

  def setUp(self):
    ebuild = 'chromeos-chrome-%s.ebuild'
    self.stable_chrome_version = '4.3.2.1'
    self.new_chrome_version = '4.3.2.2'
    self.stable_revision = 1
    stable_version = '%s_rc-r%d' % (self.stable_chrome_version,
                                    self.stable_revision)

    self.package_dir = os.path.join(self.tempdir, constants.CHROME_CP)
    osutils.SafeMakedirs(self.package_dir)

    self.stable_path = os.path.join(self.package_dir, ebuild % stable_version)
    self.unstable_path = os.path.join(self.package_dir, ebuild % '9999')

    osutils.WriteFile(self.stable_path, 'KEYWORDS=*\n')
    osutils.WriteFile(self.unstable_path, 'KEYWORDS=~*\n')

    # Avoid chroot interactions for the tests.
    self.PatchObject(uprev_lib, 'clean_stale_packages')

  def test_no_change(self):
    """Test a no-change uprev."""
    # No changes should be made when the stable and unstable ebuilds match.
    manager = uprev_lib.UprevChromeManager(
        self.stable_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    self.assertFalse(manager.modified_ebuilds)

  def test_older_version(self):
    """Test uprevving to an older version."""
    manager = uprev_lib.UprevChromeManager('1.2.3.4', overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    self.assertFalse(manager.modified_ebuilds)

  def test_new_version(self):
    """Test a new chrome version."""
    # The stable ebuild should be replaced with one of the new version.
    manager = uprev_lib.UprevChromeManager(
        self.new_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    # The old one should be deleted and the new one should exist.
    new_path = self.stable_path.replace(self.stable_chrome_version,
                                        self.new_chrome_version)
    self.assertCountEqual([self.stable_path, new_path],
                          manager.modified_ebuilds)
    self.assertExists(new_path)
    self.assertNotExists(self.stable_path)

    new_ebuild = uprev_lib.ChromeEBuild(new_path)
    expected_version = '%s_rc-r1' % self.new_chrome_version
    self.assertEqual(expected_version, new_ebuild.version)

  def test_uprev(self):
    """Test a revision bump."""
    # Make the contents different to force the uprev.
    osutils.WriteFile(self.unstable_path, 'IUSE=""', mode='a')
    manager = uprev_lib.UprevChromeManager(
        self.stable_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    new_path = self.stable_path.replace('-r%d' % self.stable_revision,
                                        '-r%d' % (self.stable_revision + 1))

    self.assertCountEqual([self.stable_path, new_path],
                          manager.modified_ebuilds)
    self.assertExists(new_path)
    self.assertNotExists(self.stable_path)

    new_ebuild = uprev_lib.ChromeEBuild(new_path)
    expected_version = '%s_rc-r%d' % (self.stable_chrome_version,
                                      self.stable_revision + 1)
    self.assertEqual(expected_version, new_ebuild.version)


class UprevManagerTest(cros_test_lib.MockTestCase):
  """UprevManager tests."""

  def test_clean_stale_packages_no_chroot(self):
    """Test no chroot skip."""
    manager = uprev_lib.UprevOverlayManager([], None)
    self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    # TODO(crbug/1065172): Invalid assertion that had previously been mocked.
    # patch.assert_not_called()

  def test_clean_stale_packages_chroot_not_exists(self):
    """Cannot run the commands when the chroot does not exist."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=False)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    # TODO(crbug/1065172): Invalid assertion that had previously been mocked.
    # patch.assert_not_called()

  def test_clean_stale_packages_no_build_targets(self):
    """Make sure it behaves as expected with no build targets provided."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_called_once_with(mock.ANY, [[None]])

  def test_clean_stale_packages_with_boards(self):
    """Test it cleans all boards as well as the chroot."""
    targets = ['board1', 'board2']
    build_targets = [BuildTarget(t) for t in targets]
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([],
                                            None,
                                            chroot=chroot,
                                            build_targets=build_targets)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    patch.assert_called_once_with(mock.ANY, [[t] for t in targets + [None]])


def test_find_chrome_ebuilds(overlay_stack):
  """Test that chrome ebuilds can be discovered in the test overlay."""

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='9999', keywords='~*')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='78.0.3876.1-r1')
  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  unstable, stable = uprev_lib.find_chrome_ebuilds(
      overlay.path / 'chromeos-base' / 'chromeos-chrome')
  assert unstable
  assert stable


def test_find_chrome_stable_candidate(overlay_stack):
  """Test that a stable uprev candidate can be chosen in the expected case."""
  NEW_CHROME_VERSION = '80.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='9999', keywords='~*')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='78.0.3876.0_rc-r1')
  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  _unstable, stable = uprev_lib.find_chrome_ebuilds(
      overlay.path / 'chromeos-base' / 'chromeos-chrome')
  assert stable
  uprev_manager = uprev_lib.UprevChromeManager(version=NEW_CHROME_VERSION)
  # pylint: disable=protected-access
  candidate = uprev_manager._find_chrome_uprev_candidate(stable)
  assert candidate


def test_basic_chrome_uprev(overlay_stack):
  """Test that the default uprev path works as expected."""
  NEW_CHROME_VERSION = '80.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='9999', keywords='~*')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='78.0.3876.0_rc-r1')
  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  uprev_manager = uprev_lib.UprevChromeManager(
      version=NEW_CHROME_VERSION, overlay_dir=overlay.path)

  result = uprev_manager.uprev(constants.CHROME_CP)
  assert result
  assert result.outcome is uprev_lib.Outcome.VERSION_BUMP

  new_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='80.0.1234.0_rc-r1')

  assert new_chrome.cpv in overlay


def test_chrome_uprev_revision_bump(overlay_stack):
  """Test that an uprev with the same major version just increments revision."""
  NEW_CHROME_VERSION = '80.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base',
      'chromeos-chrome',
      version='9999',
      keywords='~*',
      depend='foo/bar')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='80.0.1234.0_rc-r1')

  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  uprev_manager = uprev_lib.UprevChromeManager(
      version=NEW_CHROME_VERSION, overlay_dir=overlay.path)

  result = uprev_manager.uprev(constants.CHROME_CP)
  assert result
  assert result.outcome is uprev_lib.Outcome.REVISION_BUMP

  expected_uprev = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='80.0.1234.0_rc-r2')

  assert expected_uprev.cpv in overlay


def test_no_chrome_uprev_same_version(overlay_stack, caplog):
  """Test that no uprev occurs when version and contents are the same."""
  NEW_CHROME_VERSION = '80.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='9999', keywords='~*')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='80.0.1234.0_rc-r1')

  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  uprev_manager = uprev_lib.UprevChromeManager(
      version=NEW_CHROME_VERSION, overlay_dir=overlay.path)

  result = uprev_manager.uprev(constants.CHROME_CP)
  assert not result
  assert result.outcome is uprev_lib.Outcome.SAME_VERSION_EXISTS

  ebuild_redundant_warning = ('Previous ebuild with same version found and '
                              'ebuild is redundant.')
  assert ebuild_redundant_warning in caplog.text


def test_no_chrome_uprev_older_version(overlay_stack, caplog):
  """Test that no uprev occurs when a newer version already exists."""
  # Intentionally older than what already exists.
  NEW_CHROME_VERSION = '55.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='9999', keywords='~*')
  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version='80.0.1234.0_rc-r1')

  overlay.add_package(unstable_chrome)
  overlay.add_package(stable_chrome)

  uprev_manager = uprev_lib.UprevChromeManager(
      version=NEW_CHROME_VERSION, overlay_dir=overlay.path)

  result = uprev_manager.uprev(constants.CHROME_CP)
  assert not result
  assert result.outcome is uprev_lib.Outcome.NEWER_VERSION_EXISTS

  newer_version_warning = (
      'A chrome ebuild candidate with a higher version than the '
      'requested uprev version was found.')
  assert newer_version_warning in caplog.messages
  assert 'Candidate version found: 80.0.1234.0' in caplog.messages


def test_chrome_uprev_no_existing_stable(overlay_stack):
  """Test that an uprev generates a stable ebuild if one doesn't exist yet."""
  NEW_CHROME_VERSION = '80.0.1234.0'

  overlay, = overlay_stack(1)
  unstable_chrome = cr.test.Package(
      'chromeos-base',
      'chromeos-chrome',
      version='9999',
      keywords='~*',
      depend='foo/bar')

  overlay.add_package(unstable_chrome)

  uprev_manager = uprev_lib.UprevChromeManager(
      version=NEW_CHROME_VERSION, overlay_dir=overlay.path)

  result = uprev_manager.uprev(constants.CHROME_CP)
  assert result
  assert result.outcome is uprev_lib.Outcome.NEW_EBUILD_CREATED

  stable_chrome = cr.test.Package(
      'chromeos-base', 'chromeos-chrome', version=f'{NEW_CHROME_VERSION}_rc-r1')

  assert stable_chrome.cpv in overlay

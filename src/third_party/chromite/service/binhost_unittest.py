# -*- coding: utf-8 -*-path + os.sep)
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the binhost.py service."""

from __future__ import print_function

import os

from chromite.lib import binpkg
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util
from chromite.service import binhost

class SetBinhostTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for SetBinhost."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)

    self.public_conf_dir = os.path.join(
        self.tempdir, constants.PUBLIC_BINHOST_CONF_DIR, 'target')
    osutils.SafeMakedirs(self.public_conf_dir)

    self.private_conf_dir = os.path.join(
        self.tempdir, constants.PRIVATE_BINHOST_CONF_DIR, 'target')
    osutils.SafeMakedirs(self.private_conf_dir)

  def tearDown(self):
    osutils.EmptyDir(self.tempdir)

  def testSetBinhostPublic(self):
    """SetBinhost returns correct public path and updates conf file."""
    actual = binhost.SetBinhost(
        'coral', 'BINHOST_KEY', 'gs://prebuilts', private=False)
    expected = os.path.join(self.public_conf_dir, 'coral-BINHOST_KEY.conf')
    self.assertEqual(actual, expected)
    self.assertEqual(osutils.ReadFile(actual), 'BINHOST_KEY="gs://prebuilts"')

  def testSetBinhostPrivate(self):
    """SetBinhost returns correct private path and updates conf file."""
    actual = binhost.SetBinhost('coral', 'BINHOST_KEY', 'gs://prebuilts')
    expected = os.path.join(self.private_conf_dir, 'coral-BINHOST_KEY.conf')
    self.assertEqual(actual, expected)
    self.assertEqual(osutils.ReadFile(actual), 'BINHOST_KEY="gs://prebuilts"')

  def testSetBinhostEmptyConf(self):
    """SetBinhost rejects existing but empty conf files."""
    conf_path = os.path.join(self.private_conf_dir, 'multi-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, ' ')
    with self.assertRaises(ValueError):
      binhost.SetBinhost('multi', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhostMultilineConf(self):
    """SetBinhost rejects existing multiline conf files."""
    conf_path = os.path.join(self.private_conf_dir, 'multi-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, '\n'.join(['A="foo"', 'B="bar"']))
    with self.assertRaises(ValueError):
      binhost.SetBinhost('multi', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhhostBadConfLine(self):
    """SetBinhost rejects existing conf files with malformed lines."""
    conf_path = os.path.join(self.private_conf_dir, 'bad-BINHOST_KEY.conf')
    osutils.WriteFile(conf_path, 'bad line')
    with self.assertRaises(ValueError):
      binhost.SetBinhost('bad', 'BINHOST_KEY', 'gs://blah')

  def testSetBinhostMismatchedKey(self):
    """SetBinhost rejects existing conf files with a mismatched key."""
    conf_path = os.path.join(self.private_conf_dir, 'bad-key-GOOD_KEY.conf')
    osutils.WriteFile(conf_path, 'BAD_KEY="https://foo.bar"')
    with self.assertRaises(KeyError):
      binhost.SetBinhost('bad-key', 'GOOD_KEY', 'gs://blah')


class GetPrebuiltsRootTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for GetPrebuiltsRoot."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)
    self.root = os.path.join(self.tempdir, 'chroot/build/foo/packages')
    osutils.SafeMakedirs(self.root)

  def testGetPrebuiltsRoot(self):
    """GetPrebuiltsRoot returns correct root for given build target."""
    actual = binhost.GetPrebuiltsRoot('foo')
    self.assertEqual(actual, self.root)

  def testGetPrebuiltsBadTarget(self):
    """GetPrebuiltsRoot dies on missing root (target probably not built.)"""
    with self.assertRaises(LookupError):
      binhost.GetPrebuiltsRoot('bar')


class GetPrebuiltsFilesTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for GetPrebuiltsFiles."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)
    self.root = os.path.join(self.tempdir, 'chroot/build/target/packages')
    osutils.SafeMakedirs(self.root)

  def testGetPrebuiltsFiles(self):
    """GetPrebuiltsFiles returns all archives for all packages."""
    packages_content = """\
ARCH: amd64

CPV: package/prebuilt_a

CPV: package/prebuilt_b
    """
    osutils.WriteFile(os.path.join(self.root, 'Packages'), packages_content)
    osutils.WriteFile(os.path.join(self.root, 'package/prebuilt_a.tbz2'), 'a',
                      makedirs=True)
    osutils.WriteFile(os.path.join(self.root, 'package/prebuilt_b.tbz2'), 'b')

    actual = binhost.GetPrebuiltsFiles(self.root)
    expected = ['package/prebuilt_a.tbz2', 'package/prebuilt_b.tbz2']
    self.assertEqual(actual, expected)

  def testGetPrebuiltsFilesWithDebugSymbols(self):
    """GetPrebuiltsFiles returns debug symbols archive if specified in index."""
    packages_content = """\
ARCH: amd64

CPV: package/prebuilt
DEBUG_SYMBOLS: yes
    """
    osutils.WriteFile(os.path.join(self.root, 'Packages'), packages_content)
    osutils.WriteFile(os.path.join(self.root, 'package/prebuilt.tbz2'), 'foo',
                      makedirs=True)
    osutils.WriteFile(os.path.join(self.root, 'package/prebuilt.debug.tbz2'),
                      'debug', makedirs=True)

    actual = binhost.GetPrebuiltsFiles(self.root)
    expected = ['package/prebuilt.tbz2', 'package/prebuilt.debug.tbz2']
    self.assertEqual(actual, expected)

  def testGetPrebuiltsFilesBadFile(self):
    """GetPrebuiltsFiles dies if archive file does not exist."""
    packages_content = """\
ARCH: amd64

CPV: package/prebuilt
    """
    osutils.WriteFile(os.path.join(self.root, 'Packages'), packages_content)

    with self.assertRaises(LookupError):
      binhost.GetPrebuiltsFiles(self.root)


class UpdatePackageIndexTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for UpdatePackageIndex."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=self.tempdir)
    self.root = os.path.join(self.tempdir, 'chroot/build/target/packages')
    osutils.SafeMakedirs(self.root)

  def testAbsoluteUploadPath(self):
    """Test UpdatePackageIndex raises an error for absolute paths."""
    with self.assertRaises(AssertionError):
      binhost.UpdatePackageIndex(self.root, 'gs://chromeos-prebuilt', '/target')

  def testUpdatePackageIndex(self):
    """UpdatePackageIndex writes updated file to disk."""
    packages_content = """\
ARCH: amd64
TTL: 0

CPV: package/prebuilt
    """
    osutils.WriteFile(os.path.join(self.root, 'Packages'), packages_content)

    binhost.UpdatePackageIndex(self.root, 'gs://chromeos-prebuilt', 'target/')

    actual = binpkg.GrabLocalPackageIndex(self.root)
    self.assertEqual(actual.header['URI'], 'gs://chromeos-prebuilt')
    self.assertEqual(int(actual.header['TTL']), 60 * 60 * 24 * 365)
    self.assertEqual(
        actual.packages,
        [{'CPV': 'package/prebuilt', 'PATH': 'target/package/prebuilt.tbz2'}])

class RegenBuildCacheTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for RegenBuildCache."""

  def testCallsRegenPortageCache(self):
    """Test that overlays=None works."""
    overlays_found = [os.path.join(self.tempdir, 'path/to')]
    for o in overlays_found:
      osutils.SafeMakedirs(o)
    find_overlays = self.PatchObject(
        portage_util, 'FindOverlays', return_value=overlays_found)
    run_tasks = self.PatchObject(parallel, 'RunTasksInProcessPool')

    binhost.RegenBuildCache(None, self.tempdir)
    find_overlays.assert_called_once_with(None, buildroot=self.tempdir)
    run_tasks.assert_called_once_with(portage_util.RegenCache, [overlays_found])

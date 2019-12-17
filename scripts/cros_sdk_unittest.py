# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for cros_sdk."""

from __future__ import print_function

import os
import subprocess
import unittest

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import retry_util
from chromite.lib import sudo
from chromite.scripts import cros_sdk


# This long decorator triggers a false positive in the docstring test.
# https://github.com/PyCQA/pylint/issues/3077
# pylint: disable=bad-docstring-quotes
@unittest.skipIf(cros_build_lib.IsInsideChroot(),
                 'Tests only make sense outside the chroot')
class CrosSdkPrerequisitesTest(cros_test_lib.TempDirTestCase):
  """Tests for required packages on the host machine.

  These are not real unit tests.  If these tests fail, it means your machine is
  missing necessary commands to support image-backed chroots.  Run cros_sdk in
  a terminal to get info about what you need to install.
  """

  def testLvmCommandsPresent(self):
    """Check for commands from the lvm2 package."""
    with sudo.SudoKeepAlive():
      cmd = ['lvs', '--version']
      result = cros_build_lib.run(cmd, check=False)
      self.assertEqual(result.returncode, 0)

  def testThinProvisioningToolsPresent(self):
    """Check for commands from the thin-provisioning-tools package."""
    with sudo.SudoKeepAlive():
      cmd = ['thin_check', '-V']
      result = cros_build_lib.run(cmd, check=False)
      self.assertEqual(result.returncode, 0)

  def testLosetupCommandPresent(self):
    """Check for commands from the mount package."""
    with sudo.SudoKeepAlive():
      cmd = ['losetup', '--help']
      result = cros_build_lib.run(cmd, check=False)
      self.assertEqual(result.returncode, 0)


class CrosSdkUtilsTest(cros_test_lib.MockTempDirTestCase):
  """Tests for misc util funcs."""

  def testGetArchStageTarballs(self):
    """Basic test of GetArchStageTarballs."""
    self.assertCountEqual([
        'https://storage.googleapis.com/chromiumos-sdk/cros-sdk-123.tar.xz',
        'https://storage.googleapis.com/chromiumos-sdk/cros-sdk-123.tbz2',
    ], cros_sdk.GetArchStageTarballs('123'))

  def testFetchRemoteTarballsEmpty(self):
    """Test FetchRemoteTarballs with no results."""
    m = self.PatchObject(retry_util, 'RunCurl')
    with self.assertRaises(ValueError):
      cros_sdk.FetchRemoteTarballs(self.tempdir, [], 'tarball')
    m.return_value = cros_build_lib.CommandResult(stdout=b'Foo: bar\n')
    with self.assertRaises(ValueError):
      cros_sdk.FetchRemoteTarballs(self.tempdir, ['gs://x.tar'], 'tarball')

  def testFetchRemoteTarballsSuccess(self):
    """Test FetchRemoteTarballs with a successful download."""
    curl = cros_build_lib.CommandResult(stdout=(
        b'HTTP/1.0 200\n'
        b'Foo: bar\n'
        b'Content-Length: 100\n'
    ))
    self.PatchObject(retry_util, 'RunCurl', return_value=curl)
    self.assertEqual(
        os.path.join(self.tempdir, 'tar'),
        cros_sdk.FetchRemoteTarballs(self.tempdir, ['gs://x/tar'], 'tarball'))


@unittest.skipIf(cros_build_lib.IsInsideChroot(),
                 'Tests only make sense outside the chroot')
@unittest.skip(
    'These are triggering hangs inside lvm when run through run_tests. '
    'https://crbug.com/764335')
class CrosSdkSnapshotTest(cros_test_lib.TempDirTestCase):
  """Tests for the snapshot functionality in cros_sdk."""

  def setUp(self):
    with sudo.SudoKeepAlive():
      # Create just enough of a chroot to fool cros_sdk into accepting it.
      self.chroot = os.path.join(self.tempdir, 'chroot')
      cros_sdk_lib.MountChroot(self.chroot, create=True)
      logging.debug('Chroot mounted on %s', self.chroot)

      chroot_etc = os.path.join(self.chroot, 'etc')
      osutils.SafeMakedirsNonRoot(chroot_etc)

      self.chroot_version_file = os.path.join(chroot_etc, 'cros_chroot_version')
      osutils.Touch(self.chroot_version_file, makedirs=True)

  def tearDown(self):
    with sudo.SudoKeepAlive():
      cros_sdk_lib.CleanupChrootMount(self.chroot, delete=True)

  def _crosSdk(self, args):
    cmd = ['cros_sdk', '--chroot', self.chroot]
    cmd.extend(args)

    try:
      result = cros_build_lib.run(
          cmd, print_cmd=False, capture_output=True, check=False,
          stderr=subprocess.STDOUT)
    except cros_build_lib.RunCommandError as e:
      raise SystemExit('Running %r failed!: %s' % (cmd, e))

    return result.returncode, result.output

  def testSnapshotsRequireImage(self):
    code, output = self._crosSdk(['--snapshot-list', '--nouse-image'])
    self.assertNotEqual(code, 0)
    self.assertIn('Snapshot operations are not compatible with', output)

    code, output = self._crosSdk(['--snapshot-delete', 'test', '--nouse-image'])
    self.assertNotEqual(code, 0)
    self.assertIn('Snapshot operations are not compatible with', output)

    code, output = self._crosSdk(['--snapshot-create', 'test', '--nouse-image'])
    self.assertNotEqual(code, 0)
    self.assertIn('Snapshot operations are not compatible with', output)

    code, output = self._crosSdk(['--snapshot-restore', 'test',
                                  '--nouse-image'])
    self.assertNotEqual(code, 0)
    self.assertIn('Snapshot operations are not compatible with', output)

  def testSnapshotWithDeleteChroot(self):
    code, output = self._crosSdk(['--delete', '--snapshot-list'])
    self.assertNotEqual(code, 0)
    self.assertIn('Trying to enter or snapshot the chroot', output)

    code, output = self._crosSdk(['--delete', '--snapshot-delete', 'test'])
    self.assertNotEqual(code, 0)
    self.assertIn('Trying to enter or snapshot the chroot', output)

    code, output = self._crosSdk(['--delete', '--snapshot-create', 'test'])
    self.assertNotEqual(code, 0)
    self.assertIn('Trying to enter or snapshot the chroot', output)

    code, output = self._crosSdk(['--delete', '--snapshot-restore', 'test'])
    self.assertNotEqual(code, 0)
    self.assertIn('Trying to enter or snapshot the chroot', output)

  def testEmptySnapshotList(self):
    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output, '')

  def testOneSnapshot(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output.strip(), 'test')

  def testMultipleSnapshots(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    code, _ = self._crosSdk(['--snapshot-create', 'test2'])
    self.assertEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertSetEqual(set(output.strip().split('\n')), {'test', 'test2'})

  def testCantCreateSameSnapshotTwice(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertNotEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output.strip(), 'test')

  def testCreateSnapshotMountsAsNeeded(self):
    with sudo.SudoKeepAlive():
      cros_sdk_lib.CleanupChrootMount(self.chroot)

    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    self.assertExists(self.chroot_version_file)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output.strip(), 'test')

  def testDeleteGoodSnapshot(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    code, _ = self._crosSdk(['--snapshot-create', 'test2'])
    self.assertEqual(code, 0)

    code, _ = self._crosSdk(['--snapshot-delete', 'test'])
    self.assertEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output.strip(), 'test2')

  def testDeleteMissingSnapshot(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    code, _ = self._crosSdk(['--snapshot-create', 'test2'])
    self.assertEqual(code, 0)

    code, _ = self._crosSdk(['--snapshot-delete', 'test3'])
    self.assertEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertSetEqual(set(output.strip().split('\n')), {'test', 'test2'})

  def testDeleteAndCreateSnapshot(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)
    code, _ = self._crosSdk(['--snapshot-create', 'test2'])
    self.assertEqual(code, 0)

    code, _ = self._crosSdk(['--snapshot-create', 'test',
                             '--snapshot-delete', 'test'])
    self.assertEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertSetEqual(set(output.strip().split('\n')), {'test', 'test2'})

  def testRestoreSnapshot(self):
    with sudo.SudoKeepAlive():
      test_file = os.path.join(self.chroot, 'etc', 'test_file')
      osutils.Touch(test_file)

      code, _ = self._crosSdk(['--snapshot-create', 'test'])
      self.assertEqual(code, 0)

      osutils.SafeUnlink(test_file)

      code, _ = self._crosSdk(['--snapshot-restore', 'test'])
      self.assertEqual(code, 0)
      self.assertTrue(cros_sdk_lib.MountChroot(self.chroot, create=False))
      self.assertExists(test_file)

      code, output = self._crosSdk(['--snapshot-list'])
      self.assertEqual(code, 0)
      self.assertEqual(output, '')

  def testRestoreAndCreateSnapshot(self):
    with sudo.SudoKeepAlive():
      test_file = os.path.join(self.chroot, 'etc', 'test_file')
      osutils.Touch(test_file)

      code, _ = self._crosSdk(['--snapshot-create', 'test'])
      self.assertEqual(code, 0)

      osutils.SafeUnlink(test_file)

      code, _ = self._crosSdk(['--snapshot-restore', 'test',
                               '--snapshot-create', 'test'])
      self.assertEqual(code, 0)
      self.assertTrue(cros_sdk_lib.MountChroot(self.chroot, create=False))
      self.assertExists(test_file)

      code, output = self._crosSdk(['--snapshot-list'])
      self.assertEqual(code, 0)
      self.assertEqual(output.strip(), 'test')

  def testDeleteCantRestoreSameSnapshot(self):
    code, _ = self._crosSdk(['--snapshot-create', 'test'])
    self.assertEqual(code, 0)

    code, _ = self._crosSdk(['--snapshot-delete', 'test',
                             '--snapshot-restore', 'test'])
    self.assertNotEqual(code, 0)

    code, output = self._crosSdk(['--snapshot-list'])
    self.assertEqual(code, 0)
    self.assertEqual(output.strip(), 'test')

  def testCantRestoreInvalidSnapshot(self):
    with sudo.SudoKeepAlive():
      test_file = os.path.join(self.chroot, 'etc', 'test_file')
      osutils.Touch(test_file)

    code, _ = self._crosSdk(['--snapshot-restore', 'test'])
    self.assertNotEqual(code, 0)
    # Failed restore leaves the existing snapshot in place.
    self.assertExists(test_file)

  def testRestoreSnapshotMountsAsNeeded(self):
    with sudo.SudoKeepAlive():
      test_file = os.path.join(self.chroot, 'etc', 'test_file')
      osutils.Touch(test_file)

      code, _ = self._crosSdk(['--snapshot-create', 'test'])
      self.assertEqual(code, 0)

      osutils.SafeUnlink(test_file)

      cros_sdk_lib.CleanupChrootMount(self.chroot)

      code, _ = self._crosSdk(['--snapshot-restore', 'test'])
      self.assertEqual(code, 0)
      self.assertExists(test_file)

      code, output = self._crosSdk(['--snapshot-list'])
      self.assertEqual(code, 0)
      self.assertEqual(output, '')

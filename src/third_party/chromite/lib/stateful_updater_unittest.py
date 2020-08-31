# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for StatefulUpdater."""

from __future__ import print_function

import os
import shutil

import mock

from chromite.lib import cros_test_lib
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import remote_access
from chromite.lib import stateful_updater
from chromite.lib import partial_mock

# pylint: disable=protected-access


class ChromiumOSDeviceMock(partial_mock.PartialMock):
  """Provides a context where RemoteDevice run commands are run locally."""

  TARGET = 'chromite.lib.remote_access.ChromiumOSDevice'
  ATTRS = ('run', 'CopyToDevice')

  # pylint: disable=unused-argument
  def run(self, inst, *args, **kwargs):
    """Partial mock for ChromiumOSDevice.run."""
    return cros_build_lib.run(*args, **kwargs)

  def CopyToDevice(self, inst, src, dst, mode, **kwargs):
    """Partial mock for ChromiumOSDevice.CopyToDevice."""
    return shutil.copy2(src, dst)


class StatefulUpdaterTest(cros_test_lib.MockTempDirTestCase):
  """A class for testing StatefulUpdater."""

  def setUp(self):
    """Sets up the objects needed for testing."""
    self._CreateStatefulUpdate()
    self._stateful_dir = os.path.join(self.tempdir, 'target')
    osutils.SafeMakedirs(self._stateful_dir)

    self.StartPatcher(ChromiumOSDeviceMock())

  def _CreateStatefulUpdate(self):
    """Creates a stateful update tar file so we can test it."""
    self._payload = os.path.join(self.tempdir, 'stateful.tgz')

    tmp_stateful = os.path.join(self.tempdir, 'temp-stateful')
    stateful_dirs = ('var_new', 'dev_image_new')
    for d in stateful_dirs:
      osutils.SafeMakedirs(os.path.join(tmp_stateful, d))

    cros_build_lib.CreateTarball(self._payload, tmp_stateful,
                                 compression=cros_build_lib.COMP_GZIP,
                                 inputs=stateful_dirs)
    self.assertExists(self._payload)

  def testTargetStatefulUpdateFileDoNotExist(self):
    """Tests that we raise error if the target stateful file doesn't exist."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(device)
      with self.assertRaises(stateful_updater.Error):
        updater.Update('/foo/path')

  def testUpdateStandard(self):
    """Tests Update function with default arguments."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(
          device, stateful_dir=self._stateful_dir)

      updater.Update(self._payload)
      self.assertEqual(osutils.ReadFile(os.path.join(
          self._stateful_dir, updater._UPDATE_TYPE_FILE)), '')

  def testUpdateClobber(self):
    """Tests Update function with default arguments."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(
          device, stateful_dir=self._stateful_dir)

      updater.Update(self._payload, update_type=updater.UPDATE_TYPE_CLOBBER)
      self.assertEqual(
          osutils.ReadFile(os.path.join(self._stateful_dir,
                                        updater._UPDATE_TYPE_FILE)),
          updater.UPDATE_TYPE_CLOBBER)

  def testInvalidUpdateType(self):
    """Tests we raise error on invalid given update type."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(device)
      with self.assertRaises(stateful_updater.Error):
        updater._MarkUpdateType('foo')

  @mock.patch.object(remote_access.RemoteDevice, 'IfPathExists',
                     return_value=False)
  def testUpdateFailed(self, _):
    """Tests Update function fails to untar the payload."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(
          device, stateful_dir=self._stateful_dir)

      with self.assertRaises(stateful_updater.Error):
        updater.Update(self._payload)

  def testReset(self):
    """Tests Reset function."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      updater = stateful_updater.StatefulUpdater(
          device, stateful_dir=self._stateful_dir)
      expected_files = (os.path.join(self.tempdir, 'target', f) for f in
                        (updater._VAR_DIR,
                         updater._DEV_IMAGE_DIR,
                         updater._UPDATE_TYPE_FILE))

      updater.Update(self._payload)

      for f in expected_files:
        self.assertExists(f)

      updater.Reset()

      # These files/directories should be deleted after the reset.
      for f in expected_files:
        self.assertNotExists(f)

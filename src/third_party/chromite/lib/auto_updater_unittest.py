# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the auto_updater module.

The main parts of unittest include:
  1. test transfer methods in ChromiumOSUpdater.
  2. test precheck methods in ChromiumOSUpdater.
  3. test update methods in ChromiumOSUpdater.
  4. test reboot and verify method in ChromiumOSUpdater.
  5. test error raising in ChromiumOSUpdater.
"""

from __future__ import print_function

import json
import os

import mock

from chromite.lib import auto_updater
from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import nebraska_wrapper
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import remote_access
from chromite.lib import remote_access_unittest
from chromite.lib import stateful_updater


class ChromiumOSBaseUpdaterMock(partial_mock.PartialCmdMock):
  """Mock out all update and verify functions in ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('RestoreStateful', 'UpdateStateful', 'UpdateRootfs',
           'SetupRootfsUpdate', 'RebootAndVerify')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def RestoreStateful(self, _inst, *_args, **_kwargs):
    """Mock out RestoreStateful."""

  def UpdateStateful(self, _inst, *_args, **_kwargs):
    """Mock out UpdateStateful."""

  def UpdateRootfs(self, _inst, *_args, **_kwargs):
    """Mock out UpdateRootfs."""

  def SetupRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""

  def RebootAndVerify(self, _inst, *_args, **_kwargs):
    """Mock out RebootAndVerify."""


class ChromiumOSBaseTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater.ChromiumOSUpdater.

  These methods have been deprecated in the auto_updater.ChromiumOSUpdater
  class. These mocks exist to test if the methods are not being called when
  ChromiumOSUpdater.RunUpdate() is called.

  TODO (sanikak): Mocked class should be removed once the deprecated methods
  are removed from auto_updater.ChromiumOSUpdater
  """
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('TransferUpdateUtilsPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferUpdateUtilsPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferStatefulUpdate."""


class CrOSLocalTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater_transfer.LocalTransfer."""
  TARGET = 'chromite.lib.auto_updater_transfer.LocalTransfer'
  ATTRS = ('TransferUpdateUtilsPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer.TransferUpdateUtilsPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer.TransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer.TransferStatefulUpdate."""


class CrOSLabTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater_transfer.LocalTransfer."""
  TARGET = 'chromite.lib.auto_updater_transfer.LabTransfer'
  ATTRS = ('TransferUpdateUtilsPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LabTransfer.TransferUpdateUtilsPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LabTransfer.TransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LabTransfer.TransferStatefulUpdate."""


class ChromiumOSPreCheckMock(partial_mock.PartialCmdMock):
  """Mock out Precheck function in auto_updater.ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('CheckRestoreStateful', '_CheckNebraskaCanRun')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def CheckRestoreStateful(self, _inst, *_args, **_kwargs):
    """Mock out auto_updater.ChromiumOSUpdater.CheckRestoreStateful."""

  def _CheckNebraskaCanRun(self, _inst, *_args, **_kwargs):
    """Mock out auto_updater.ChromiumOSUpdater._CheckNebraskaCanRun."""


class RemoteAccessMock(remote_access_unittest.RemoteShMock):
  """Mock out RemoteAccess."""

  ATTRS = ('RemoteSh', 'Rsync', 'Scp')

  def Rsync(self, *_args, **_kwargs):
    return cros_build_lib.CommandResult(returncode=0)

  def Scp(self, *_args, **_kwargs):
    return cros_build_lib.CommandResult(returncode=0)


class ChromiumOSUpdaterBaseTest(cros_test_lib.MockTempDirTestCase):
  """The base class for auto_updater.ChromiumOSUpdater test.

  In the setup, device, all transfer and update functions are mocked.
  """

  def setUp(self):
    self._payload_dir = self.tempdir
    self._base_updater_mock = self.StartPatcher(ChromiumOSBaseUpdaterMock())
    self._local_xfer_mock = self.StartPatcher(CrOSLocalTransferMock())
    self._lab_xfer_mock = self.StartPatcher(CrOSLabTransferMock())
    self._cros_transfer_mock = self.StartPatcher(ChromiumOSBaseTransferMock())
    self.PatchObject(remote_access.ChromiumOSDevice, 'Pingable',
                     return_value=True)
    m = self.StartPatcher(RemoteAccessMock())
    m.SetDefaultCmdResult()


class CrOSLocalTransferTest(ChromiumOSUpdaterBaseTest):
  """Test the transfer code path."""

  def testLocalTransferForRootfs(self):
    """Test transfer functions for rootfs update.

    When rootfs update is enabled, update-utils and rootfs update payload are
    transferred. Stateful update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._local_xfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertTrue(
          self._local_xfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._local_xfer_mock.patched['TransferStatefulUpdate'].called)

  def testLabTransferForRootfs(self):
    """Test transfer functions for rootfs update.

    When rootfs update is enabled, update-utils and rootfs update payload are
    transferred. Stateful update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LabTransfer,
          staging_server='http://0.0.0.0:8082/')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._lab_xfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertTrue(
          self._lab_xfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._lab_xfer_mock.patched['TransferStatefulUpdate'].called)

  def testLocalTransferForStateful(self):
    """Test Transfer functions' code path for stateful update.

    When stateful update is enabled, update-utils and stateful update payload
    are transferred. Rootfs update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._local_xfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._local_xfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertTrue(
          self._local_xfer_mock.patched['TransferStatefulUpdate'].called)

  def testLabTransferForStateful(self):
    """Test LabTransfer methods' code path for stateful update.

    When stateful update is enabled, update-utils and stateful update payload
    are transferred. Rootfs update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LabTransfer,
          staging_server='http://0.0.0.0:8082/')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._lab_xfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._lab_xfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertTrue(
          self._lab_xfer_mock.patched['TransferStatefulUpdate'].called)

  def testCrosTransferForRootfs(self):
    """Test auto_updater.ChromiumOSUpdater transfer methods for rootfs update.

    None of these functions should be called when auto_updater.RunUpdate() is
    called.

    TODO (sanikak): Function should be removed once the namesake deprecated
    methods are removed from auto_updater.ChromiumOSUpdater.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferStatefulUpdate'].called)

  def testCrosTransferForStateful(self):
    """Test auto_updater.ChromiumOSUpdater transfer methods for stateful update.

    None of these functions should be called when auto_updater.RunUpdate() is
    called.

    TODO (sanikak): Function should be removed once the namesake deprecated
    methods are removed from auto_updater.ChromiumOSUpdater.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferStatefulUpdate'].called)


class ChromiumOSUpdatePreCheckTest(ChromiumOSUpdaterBaseTest):
  """Test precheck function."""

  def testCheckRestoreStateful(self):
    """Test whether CheckRestoreStateful is called in update process."""
    precheck_mock = self.StartPatcher(ChromiumOSPreCheckMock())
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(precheck_mock.patched['CheckRestoreStateful'].called)

  def testCheckRestoreStatefulError(self):
    """Test CheckRestoreStateful fails with raising ChromiumOSUpdateError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(cros_build_lib, 'BooleanPrompt', return_value=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_CheckNebraskaCanRun',
                       side_effect=nebraska_wrapper.NebraskaStartupError())
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testNoPomptWithYes(self):
    """Test prompts won't be called if yes is set as True."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, yes=True,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(cros_build_lib, 'BooleanPrompt')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(cros_build_lib.BooleanPrompt.called)


class ChromiumOSUpdaterRunTest(ChromiumOSUpdaterBaseTest):
  """Test all update functions."""

  def testRestoreStateful(self):
    """Test RestoreStateful is called when it's required."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'CheckRestoreStateful',
                       return_value=True)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(self._base_updater_mock.patched['RestoreStateful'].called)

  def test_FixPayloadPropertiesFileLocal(self):
    """Tests _FixPayloadPropertiesFile() correctly fixes the properties file.

    Tests if the payload properties file gets filled with correct data when
    LocalTransfer methods are invoked internally.
    """
    payload_filename = ('payloads/chromeos_9334.58.2_reef_stable-'
                        'channel_full_test.bin-e6a3290274a5b2ae0b9f491712ae1d8')
    payload_path = os.path.join(self._payload_dir, payload_filename)
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, payload_filename=payload_filename,
          transfer_class=auto_updater_transfer.LocalTransfer)

      self.PatchObject(auto_updater_transfer.LocalTransfer,
                       'GetPayloadPropsFile',
                       return_value=payload_properties_path)
      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access

    # The payload properties file should be updated with new fields.
    props = json.loads(osutils.ReadFile(payload_properties_path))
    expected_props = {
        'appid': '',
        'is_delta': False,
        'size': os.path.getsize(payload_path),
        'target_version': '9334.58.2',
    }
    self.assertEqual(props, expected_props)

  def test_FixPayloadPropertiesFileLab(self):
    """Tests _FixPayloadPropertiesFile() correctly fixes the properties file.

    Tests if the payload properties file gets filled with correct data when
    LabTransfer methods are invoked internally.
    """
    payload_dir = 'nyan_kitty-release/R76-12345.17.0'
    payload_path = os.path.join(self.tempdir, 'test_update.gz')
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, payload_dir=payload_dir,
          transfer_class=auto_updater_transfer.LabTransfer,
          staging_server='http://0.0.0.0:8082')

      self.PatchObject(auto_updater_transfer.LabTransfer, 'GetPayloadPropsFile',
                       return_value=payload_properties_path)
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize',
                       return_value=os.path.getsize(payload_path))
      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access

    # The payload properties file should be updated with new fields.
    props = json.loads(osutils.ReadFile(payload_properties_path))
    expected_props = {
        'appid': '',
        'is_delta': False,
        'size': os.path.getsize(payload_path),
        'target_version': '12345.17.0',
    }
    self.assertEqual(props, expected_props)

  def testRunRootfs(self):
    """Test the update functions are called correctly.

    SetupRootfsUpdate and UpdateRootfs are called for rootfs update.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertTrue(self._base_updater_mock.patched['UpdateRootfs'].called)
      self.assertFalse(self._base_updater_mock.patched['UpdateStateful'].called)

  def testRunStateful(self):
    """Test the update functions are called correctly.

    Only UpdateStateful is called for stateful update.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertFalse(self._base_updater_mock.patched['UpdateRootfs'].called)
      self.assertTrue(self._base_updater_mock.patched['UpdateStateful'].called)

  def testMismatchedAppId(self):
    """Tests if App ID is set to empty when there's a mismatch."""
    self._payload_dir = self.tempdir

    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      type(CrOS_AU.device).app_id = mock.PropertyMock(return_value='different')
      resolved_appid = CrOS_AU.ResolveAPPIDMismatchIfAny('helloworld!')
      self.assertEqual(resolved_appid, '')

  def testMatchedAppId(self):
    """Tests if App ID is unchanged when there's a match."""
    self._payload_dir = self.tempdir

    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      type(CrOS_AU.device).app_id = mock.PropertyMock(return_value='same')
      resolved_appid = CrOS_AU.ResolveAPPIDMismatchIfAny('same')
      self.assertEqual(resolved_appid, 'same')

  def testCreateTransferObjectError(self):
    """Test ChromiumOSUpdater.CreateTransferObject method.

    Tests if the method throws ChromiumOSUpdater error when a class that is not
    a subclass of auto_updater_transfer.Transfer is passed as value for the
    transfer_class argument.
    """
    class NotATransferSubclass(object):
      """Dummy class for testing ChromiumOSUpdater.CreateTransferObject."""

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      self.assertRaises(AssertionError,
                        auto_updater.ChromiumOSUpdater, device=device,
                        build_name=None, payload_dir=self._payload_dir,
                        staging_server='http://0.0.0.0:8082/',
                        transfer_class=NotATransferSubclass)

  def testCheckPayloadsLocalTransferObject(self):
    """Tests if LocalTransfer.CheckPayloads is called.

    This unittest can be removed once all callers of
    ChromiumOSUpdater.CheckPayloads are removed.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater_transfer.LocalTransfer, 'CheckPayloads')
      CrOS_AU.CheckPayloads()
      self.assertTrue(auto_updater_transfer.LocalTransfer.CheckPayloads.called)

  def testCheckPayloadsLabTransferObject(self):
    """Tests if LabTransfer.CheckPayloads is called.

    This unittest can be removed once all callers of
    ChromiumOSUpdater.CheckPayloads are removed.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          staging_server='http://0.0.0.0:8082/',
          transfer_class=auto_updater_transfer.LabTransfer)
      self.PatchObject(auto_updater_transfer.LabTransfer, 'CheckPayloads')
      CrOS_AU.CheckPayloads()
      self.assertTrue(auto_updater_transfer.LabTransfer.CheckPayloads.called)

  def test_FixPayloadLocalTransfer(self):
    """Tests if correct LocalTransfer methods are called."""
    payload_filename = ('payloads/chromeos_9334.58.2_reef_stable-'
                        'channel_full_test.bin-e6a3290274a5b2ae0b9f491712ae1d8')
    payload_path = os.path.join(self.tempdir, payload_filename)
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, payload_dir=self._payload_dir,
          payload_filename=payload_filename,
          transfer_class=auto_updater_transfer.LocalTransfer)

      self.PatchObject(
          auto_updater_transfer.LocalTransfer, 'GetPayloadPropsFile',
          return_value=payload_properties_path)
      self.PatchObject(
          auto_updater_transfer.LocalTransfer, 'GetPayloadProps',
          return_value={'size': '123', 'image_version': '99999.9.9'})

      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access
      self.assertTrue(
          auto_updater_transfer.LocalTransfer.GetPayloadPropsFile.called)
      self.assertTrue(
          auto_updater_transfer.LocalTransfer.GetPayloadProps.called)

  def test_FixPayloadLabTransfer(self):
    """Tests if correct LabTransfer methods are called."""
    payload_dir = 'nyan_kitty-release/R76-12345.17.0'
    payload_path = os.path.join(self.tempdir, 'test_update.gz')
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, payload_dir=payload_dir,
          transfer_class=auto_updater_transfer.LabTransfer,
          staging_server='http://0.0.0.0:8082')

      self.PatchObject(
          auto_updater_transfer.LabTransfer, 'GetPayloadPropsFile',
          return_value=payload_properties_path)
      self.PatchObject(
          auto_updater_transfer.LabTransfer, 'GetPayloadProps',
          return_value={'size': '123', 'image_version': '99999.9.9'})

      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access
      self.assertTrue(
          auto_updater_transfer.LabTransfer.GetPayloadPropsFile.called)
      self.assertTrue(
          auto_updater_transfer.LabTransfer.GetPayloadProps.called)


class ChromiumOSUpdaterVerifyTest(ChromiumOSUpdaterBaseTest):
  """Test verify function in ChromiumOSUpdater."""

  def testRebootAndVerifyWithRootfsAndReboot(self):
    """Test RebootAndVerify if rootfs update and reboot are enabled."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(self._base_updater_mock.patched['RebootAndVerify'].called)

  def testRebootAndVerifyWithoutReboot(self):
    """Test RebootAndVerify doesn't run if reboot is unenabled."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, reboot=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._base_updater_mock.patched['RebootAndVerify'].called)


class ChromiumOSErrorTest(cros_test_lib.MockTestCase):
  """Base class for error test in auto_updater."""

  def setUp(self):
    """Mock device's functions for update.

    Not mock the class ChromiumOSDevice, in order to raise the errors that
    caused by a inner function of the device's base class, like 'run'.
    """
    self._payload_dir = ''
    self.PatchObject(remote_access.RemoteDevice, 'Pingable', return_value=True)
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', new='')
    self.PatchObject(remote_access.RemoteDevice, 'Reboot')
    self.PatchObject(remote_access.RemoteDevice, 'Cleanup')


class ChromiumOSUpdaterRunErrorTest(ChromiumOSErrorTest):
  """Test whether error is correctly reported during update process."""

  def setUp(self):
    """Mock device's function, and transfer/precheck functions for update.

    Since cros_test_lib.MockTestCase run all setUp & tearDown methods in the
    inheritance tree, we don't call super().setUp().
    """
    self.StartPatcher(CrOSLocalTransferMock())
    self.StartPatcher(ChromiumOSPreCheckMock())

  def prepareRootfsUpdate(self):
    """Prepare work for test errors in rootfs update."""
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev')
    self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'PrintLog')
    self.PatchObject(remote_access.RemoteDevice, 'CopyFromDevice')

  def testRestoreStatefulError(self):
    """Test ChromiumOSUpdater.RestoreStateful with raising exception.

    Nebraska still cannot run after restoring stateful partition will lead
    to ChromiumOSUpdateError.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'CheckRestoreStateful',
                       return_value=True)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'RunUpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'PreSetupStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'TransferStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'ResetStatefulPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateStateful')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'PostCheckStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_IsUpdateUtilsPackageInstalled')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_CheckNebraskaCanRun',
                       side_effect=nebraska_wrapper.NebraskaStartupError())
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testSetupRootfsUpdateError(self):
    """Test ChromiumOSUpdater.SetupRootfsUpdate with raising exception.

    RootfsUpdateError is raised if it cannot get status from GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_StartUpdateEngineIfNotRunning')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       return_value=('cannot_update', ))
      self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateNebraskaError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if nebraska cannot start.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start',
                       side_effect=nebraska_wrapper.Error())
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'RevertBootPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if device fails to run rootfs update command
    'update_engine_client ...'.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start')
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'GetURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'RevertBootPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateTrackError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if it fails to track device's status by
    GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start')
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'GetURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       side_effect=ValueError('Cannot get update status'))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateIdleError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if the update engine goes to idle state
    after downloading.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start')
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'GetURL')
      mock_run = self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       side_effect=(
                           ('UPDATE_STATUS_DOWNLOADING', '0.5'),
                           ('UPDATE_STATUS_DOWNLOADING', '0.9'),
                           ('UPDATE_STATUS_FINALIZING', 0),
                           ('UPDATE_STATUS_IDLE', 0),
                       ))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      patch_join = mock.patch('os.path.join', return_value='')
      patch_sleep = mock.patch('time.sleep')
      with patch_sleep as mock_sleep, patch_join as _:
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)
        mock_sleep.assert_called()
        mock_run.assert_any_call(['cat', '/var/log/update_engine.log'])

  def testStatefulUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateStateful with raising exception.

    StatefulUpdateError is raised if device fails to update stateful partition.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(remote_access.ChromiumOSDevice, 'run',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.PatchObject(stateful_updater.StatefulUpdater, 'Update',
                       side_effect=stateful_updater.Error())
      self.PatchObject(stateful_updater.StatefulUpdater, 'Reset',
                       side_effect=stateful_updater.Error())
      self.assertRaises(auto_updater.StatefulUpdateError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithSameRootDev(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value='fake_path')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithRootDevEqualsNone(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testNoVerifyError(self):
    """Test RebootAndVerify won't raise any error when unable do_rootfs_update.

    If do_rootfs_update is unabled, verify logic won't be touched, which means
    no AutoUpdateVerifyError will be thrown.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False,
          transfer_class=auto_updater_transfer.LocalTransfer)
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      self.PatchObject(stateful_updater.StatefulUpdater, 'Update')

      try:
        CrOS_AU.RunUpdate()
      except auto_updater.AutoUpdateVerifyError:
        self.fail('RunUpdate raise AutoUpdateVerifyError.')

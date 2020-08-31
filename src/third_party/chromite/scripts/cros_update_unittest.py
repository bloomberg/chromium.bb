# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_update."""

from __future__ import print_function

import mock

from chromite.lib import auto_updater
from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import remote_access
from chromite.scripts import cros_update


# pylint: disable=protected-access

class CrosUpdateTest(cros_test_lib.RunCommandTestCase):
  """Tests build_dlc utility functions."""

  def setUp(self):
    """Setup an instance of CrOSUpdateTrigger."""
    self._cros_update_trigger = cros_update.CrOSUpdateTrigger(
        'foo-host-name', 'foo-build-name', 'foo-static-dir',
        original_build='foo-original-build', static_url='foo-static',
        devserver_url='foo-devserver-url')
    self._cros_updater = auto_updater.ChromiumOSUpdater(
        mock.MagicMock(work_dir='foo-dir'), 'foo-build-name', 'foo-payload-dir',
        transfer_class=auto_updater_transfer.LocalTransfer)

  def test_GetOriginalPayloadDir(self):
    """Tests getting the original payload directory."""
    self.assertEqual(self._cros_update_trigger._GetOriginalPayloadDir(),
                     'foo-static-dir/stable-channel/foo-original-build')

    self._cros_update_trigger.original_build = None
    self.assertIsNone(self._cros_update_trigger._GetOriginalPayloadDir())

  def test_MakeStatusUrl(self):
    """Tests generating a URL to post auto update status."""
    self.assertEqual(
        self._cros_update_trigger._MakeStatusUrl(
            'foo-devserver-url', 'foo-host-name', 10),
        'foo-devserver-url/post_au_status?host_name=foo-host-name&pid=10')


  @mock.patch.object(cros_update.CrOSUpdateTrigger, '_MakeStatusUrl',
                     return_value='foo-url')
  @mock.patch.object(remote_access.RemoteDevice, 'run',
                     return_value=cros_build_lib.CommandResult(output='output'))
  def test_QuickProvision(self, run_command_call, _):
    """Tests launching quick provision."""
    cmd = ('mkdir -p /usr/local/tmp && '
           'curl -o /usr/local/tmp/quick-provision '
           'foo-static/quick-provision && '
           'bash /usr/local/tmp/quick-provision --status_url foo-url '
           'foo-build-name foo-static')
    device = remote_access.RemoteDevice('fake-hostname')
    self._cros_update_trigger._QuickProvision(device)
    run_command_call.assert_called_with(cmd, log_output=True, shell=True,
                                        capture_output=True, ssh_error_ok=True,
                                        encoding='utf-8')

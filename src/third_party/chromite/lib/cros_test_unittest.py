# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for CrOSTest."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import cros_test


# pylint: disable=protected-access
class CrOSTester(cros_test_lib.RunCommandTempDirTestCase):
  """Test cros_test.CrOSTest."""

  def setUp(self):
    """Common set up method for all tests."""
    opts = cros_test.ParseCommandLine([])
    self._tester = cros_test.CrOSTest(opts)
    self._tester._device.board = 'amd64-generic'
    self._tester._device.qemu_path = '/usr/bin/qemu-system-x86_64'
    self._tester._device.image_path = self.TempFilePath(
        'chromiumos_qemu_image.bin')
    osutils.Touch(self._tester._device.image_path)
    version_str = ('QEMU emulator version 2.6.0, Copyright (c) '
                   '2003-2008 Fabrice Bellard')
    self.rc.AddCmdResult(partial_mock.In('--version'), output=version_str)
    self.ssh_port = self._tester._device.ssh_port

  def TempFilePath(self, file_path):
    return os.path.join(self.tempdir, file_path)

  def testBasic(self):
    """Tests basic functionality."""
    self._tester.Run()
    # Check VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])
    # Wait for VM to be responsive.
    self.assertCommandContains([
        'ssh', '-p', '9222', 'root@localhost', '--', 'true'
    ])
    # Run vm_sanity.
    self.assertCommandContains([
        'ssh', '-p', '9222', 'root@localhost', '--',
        '/usr/local/autotest/bin/vm_sanity.py'
    ])

# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test service.

Handles test related functionality.
"""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib


def DebugInfoTest(sysroot_path):
  """Run the debug info tests.

  Args:
    sysroot_path (str): The sysroot being tested.

  Returns:
    bool - True iff all tests passed, False otherwise.
  """
  cmd = ['debug_info_test', os.path.join(sysroot_path, 'usr/lib/debug')]
  result = cros_build_lib.RunCommand(cmd, enter_chroot=True, error_code_ok=True)

  return result.returncode == 0

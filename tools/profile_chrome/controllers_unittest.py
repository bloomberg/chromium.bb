# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from profile_chrome import profiler

from pylib import android_commands
from pylib.device import device_utils
from pylib.device import intent


class BaseControllerTest(unittest.TestCase):
  def setUp(self):
    devices = android_commands.GetAttachedDevices()
    self.browser = 'stable'
    self.package_info = profiler.GetSupportedBrowsers()[self.browser]
    self.device = device_utils.DeviceUtils(devices[0])

    self.device.StartActivity(
        intent.Intent(activity=self.package_info.activity,
                      package=self.package_info.package),
        blocking=True)

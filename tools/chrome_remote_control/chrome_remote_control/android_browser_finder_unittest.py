# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import unittest

from chrome_remote_control import android_browser_finder
from chrome_remote_control import browser_options
from chrome_remote_control import system_stub

class AndroidBrowserFinderTest(unittest.TestCase):
  def setUp(self):
    self._stubs = system_stub.Override(android_browser_finder,
                                       ['adb_commands', 'subprocess'])

  def tearDown(self):
    self._stubs.Restore()

  def test_no_adb(self):
    options = browser_options.BrowserOptions()

    def NoAdb(*args, **kargs): # pylint: disable=W0613
      raise OSError('not found')
    self._stubs.subprocess.Popen = NoAdb
    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(0, len(browsers))

  def test_adb_no_devices(self):
    options = browser_options.BrowserOptions()

    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(0, len(browsers))


  def test_adb_permissions_error(self):
    options = browser_options.BrowserOptions()

    self._stubs.subprocess.Popen.communicate_result = (
        """List of devices attached
????????????\tno permissions""",
        """* daemon not running. starting it now on port 5037 *
* daemon started successfully *
""")

    warnings = []
    class TempFilter(logging.Filter):
      def filter(self, record):
        warnings.append(record)
        return 0
    temp_filter = TempFilter()

    try:
      logger = logging.getLogger()
      logger.addFilter(temp_filter)

      browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    finally:
      logger.removeFilter(temp_filter)
    self.assertEquals(3, len(warnings))
    self.assertEquals(0, len(browsers))


  def test_adb_two_devices(self):
    options = browser_options.BrowserOptions()

    self._stubs.adb_commands.attached_devices = ['015d14fec128220c',
                                                 '015d14fec128220d']

    warnings = []
    class TempFilter(logging.Filter):
      def filter(self, record):
        warnings.append(record)
        return 0
    temp_filter = TempFilter()

    try:
      logger = logging.getLogger()
      logger.addFilter(temp_filter)

      browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    finally:
      logger.removeFilter(temp_filter)
    self.assertEquals(1, len(warnings))
    self.assertEquals(0, len(browsers))

  def test_adb_one_device(self):
    options = browser_options.BrowserOptions()

    self._stubs.adb_commands.attached_devices = ['015d14fec128220c']

    def OnPM(args):
      assert args[0] == 'pm'
      assert args[1] == 'list'
      assert args[2] == 'packages'
      return ['package:org.chromium.content_shell',
              'package.com.google.android.setupwizard']

    self._stubs.adb_commands.shell_command_handlers['pm'] = OnPM

    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(1, len(browsers))

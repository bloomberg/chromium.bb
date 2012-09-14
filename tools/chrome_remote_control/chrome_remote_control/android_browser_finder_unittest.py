# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import unittest

from chrome_remote_control import android_browser_finder
from chrome_remote_control import browser_options

from system_stub import *

# adb not even found
# android_browser_finder not returning
class ADBCommandsStub(object):
  def __init__(self, module, device):
    self._module = module
    self._device = device
    self.is_root_enabled = True

  def RunShellCommand(self, args):
    if isinstance(args, basestring):
      import shlex
      args = shlex.split(args)
    handler = self._module.shell_command_handlers[args[0]]
    return handler(args)

  def IsRootEnabled(self):
    return self.is_root_enabled

class ADBCommandsModuleStub(object):
  def __init__(self):
    self.attached_devices = []
    self.shell_command_handlers = {}

    def ADBCommandsStubConstructor(device=None):
      return ADBCommandsStub(self, device)
    self.ADBCommands = ADBCommandsStubConstructor

  def IsAndroidSupported(self):
    return True

  def GetAttachedDevices(self):
    return self.attached_devices

  def HasForwarder(self, adb):
    return True

class AndroidBrowserFinderTest(unittest.TestCase):
  def test_no_adb(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = SubprocessModuleStub()
    def NoADB(*args, **kargs):
      raise OSError('not found')
    subprocess_stub.Popen_hook = NoADB
    browsers = android_browser_finder.FindAllAvailableBrowsers(
        options, subprocess_stub)
    self.assertEquals(0, len(browsers))

  def test_adb_no_devices(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = SubprocessModuleStub()
    popen_stub = PopenStub(('', ''))
    subprocess_stub.Popen_result = popen_stub

    adb_commands_module_stub = ADBCommandsModuleStub()
    browsers = android_browser_finder.FindAllAvailableBrowsers(
        options, subprocess_stub, adb_commands_module_stub)
    self.assertEquals(0, len(browsers))


  def test_adb_permissions_error(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = SubprocessModuleStub()
    popen_stub = PopenStub((
        """List of devices attached
????????????\tno permissions""",
        """* daemon not running. starting it now on port 5037 *
* daemon started successfully *
"""))
    subprocess_stub.Popen_result = popen_stub

    adb_commands_module_stub = ADBCommandsModuleStub()

    warnings = []
    class TempFilter(logging.Filter):
      def filter(self, record):
        warnings.append(record)
        return 0
    temp_filter = TempFilter()

    try:
      logger = logging.getLogger()
      logger.addFilter(temp_filter)

      browsers = android_browser_finder.FindAllAvailableBrowsers(
          options, subprocess_stub, adb_commands_module_stub)
    finally:
      logger.removeFilter(temp_filter)
    self.assertEquals(3, len(warnings))
    self.assertEquals(0, len(browsers))


  def test_adb_two_devices(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = SubprocessModuleStub()
    popen_stub = PopenStub(('', ''))
    subprocess_stub.Popen_result = popen_stub
    adb_commands_module_stub = ADBCommandsModuleStub()
    adb_commands_module_stub.attached_devices = ['015d14fec128220c',
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

      browsers = android_browser_finder.FindAllAvailableBrowsers(
          options, subprocess_stub, adb_commands_module_stub)
    finally:
      logger.removeFilter(temp_filter)
    self.assertEquals(1, len(warnings))
    self.assertEquals(0, len(browsers))

  def test_adb_one_device(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = SubprocessModuleStub()
    popen_stub = PopenStub(('', ''))
    subprocess_stub.Popen_result = popen_stub
    adb_commands_module_stub = ADBCommandsModuleStub()
    adb_commands_module_stub.attached_devices = ['015d14fec128220c']

    def OnPM(args):
      assert args[0] == 'pm'
      assert args[1] == 'list'
      assert args[2] == 'packages'
      return ['package:org.chromium.content_shell',
              'package.com.google.android.setupwizard']

    adb_commands_module_stub.shell_command_handlers['pm'] = OnPM

    browsers = android_browser_finder.FindAllAvailableBrowsers(
        options, subprocess_stub, adb_commands_module_stub)
    self.assertEquals(1, len(browsers))

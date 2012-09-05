# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import unittest

import android_browser_finder
import browser_options

# adb not even found
# android_browser_finder not returning

class StubSubprocess(object):
  def __init__(self):
    self.call_hook = None

  def call(self, *args, **kwargs):
    if not self.call_hook:
      raise Exception('Should not be reached.')
    return self.call_hook(*args, **kwargs)

class StubADBCommands(object):
  def __init__(self, module, device):
    self._module = module
    self._device = device

  def RunShellCommand(self, args):
    if isinstance(args, basestring):
      import shlex
      args = shlex.split(args)
    handler = self._module.shell_command_handlers[args[0]]
    return handler(args)

class StubADBCommandsModule(object):
  def __init__(self):
    self.attached_devices = []
    self.shell_command_handlers = {}

    def StubADBCommandsConstructor(device=None):
      return StubADBCommands(self, device)
    self.ADBCommands = StubADBCommandsConstructor

  def IsAndroidSupported(self):
    return True

  def GetAttachedDevices(self):
    return self.attached_devices

class AndroidBrowserFinderTest(unittest.TestCase):
  def test_no_adb(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = StubSubprocess()
    def NoADB(*args, **kargs):
      raise OSError('not found')
    subprocess_stub.call_hook = NoADB
    browsers = android_browser_finder.FindAllAvailableBrowsers(
        options, subprocess_stub)
    self.assertEquals(0, len(browsers))

  def test_adb_no_devices(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = StubSubprocess()
    subprocess_stub.call_hook = lambda *args, **kargs: 0
    adb_commands_module_stub = StubADBCommandsModule()
    browsers = android_browser_finder.FindAllAvailableBrowsers(
        options, subprocess_stub, adb_commands_module_stub)
    self.assertEquals(0, len(browsers))

  def test_adb_two_devices(self):
    options = browser_options.BrowserOptions()

    subprocess_stub = StubSubprocess()
    subprocess_stub.call_hook = lambda *args, **kargs: 0
    adb_commands_module_stub = StubADBCommandsModule()
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

    subprocess_stub = StubSubprocess()
    subprocess_stub.call_hook = lambda *args, **kargs: 0
    adb_commands_module_stub = StubADBCommandsModule()
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

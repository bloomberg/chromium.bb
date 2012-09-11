# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess
import logging
import re

import chrome_remote_control.android_browser_backend as android_browser_backend
import chrome_remote_control.adb_commands as real_adb_commands
import browser
import possible_browser


"""Finds android browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = ','.join([
    'android-content-shell',
    'android-chrome',
    'android-jb-system-chrome',
    ])

CHROME_PACKAGE = 'com.google.android.apps.chrome'
CHROME_ACTIVITY = '.Main'
CHROME_COMMAND_LINE = '/data/local/chrome-command-line'
CHROME_DEVTOOLS_REMOTE_PORT = 'localabstract:chrome_devtools_remote'

CHROME_JB_SYSTEM_PACKAGE = 'com.android.chrome'
CHROME_JB_SYSTEM_DEVTOOLS_REMOTE_PORT = 'localabstract:chrome_devtools_remote'

CONTENT_SHELL_PACKAGE = 'org.chromium.content_shell'
CONTENT_SHELL_ACTIVITY = '.ContentShellActivity'
CONTENT_SHELL_COMMAND_LINE = '/data/local/tmp/content-shell-command-line'
CONTENT_SHELL_DEVTOOLS_REMOTE_PORT = (
    'localabstract:content_shell_devtools_remote')

# adb shell pm list packages
# adb
# intents to run (pass -D url for the rest)
#   com.android.chrome/.Main
#   com.google.android.apps.chrome/.Main

class PossibleAndroidBrowser(possible_browser.PossibleBrowser):
  """A launchable android browser instance."""
  def __init__(self, type, options, *args):
    super(PossibleAndroidBrowser, self).__init__(
        type, options)
    self._args = args

  def __repr__(self):
    return 'PossibleAndroidBrowser(type=%s)' % self.type

  def Create(self):
    backend = android_browser_backend.AndroidBrowserBackend(
        self.type, self._options, *self._args)
    return browser.Browser(backend)

def FindAllAvailableBrowsers(options,
                             subprocess = real_subprocess,
                             adb_commands = real_adb_commands):
  """Finds all the desktop browsers available on this machine."""
  if not adb_commands.IsAndroidSupported():
    return []

  # See if adb even works.
  try:
    with open(real_os.devnull, 'w') as devnull:
      proc = subprocess.Popen(['adb', 'devices'],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              stdin=devnull)
      stdout, stderr = proc.communicate()
      if re.search(re.escape("????????????\tno permissions"), stdout) != None:
        logging.warning(
            ("adb devices reported a permissions error. Consider "
            "restarting adb as root:"))
        logging.warning("  adb kill-server")
        logging.warning("  sudo `which adb` devices\n\n")
  except OSError:
    logging.info('No adb command found. ' +
                 'Will not try searching for Android browsers.')
    return []

  device = None
  if not options.android_device:
    devices = adb_commands.GetAttachedDevices()
  else:
    devices = []

  if len(devices) == 0:
    logging.info('No android devices found.')
    return []

  if len(devices) > 1:
    logging.warn('Multiple devices attached. ' +
                 'Please specify a device explicitly.')
    return []

  device = devices[0]

  adb = adb_commands.ADBCommands(device=device)

  # See if adb is root
  if not adb.IsRootEnabled():
    logging.warn('ADB is not root. Please make it root by doing:')
    logging.warn(' adb root')
    return []

  packages = adb.RunShellCommand('pm list packages')
  possible_browsers = []
  if 'package:' + CONTENT_SHELL_PACKAGE in packages:
    b = PossibleAndroidBrowser('android-content-shell',
                               options, adb,
                               CONTENT_SHELL_PACKAGE, True,
                               CONTENT_SHELL_COMMAND_LINE,
                               CONTENT_SHELL_ACTIVITY,
                               CONTENT_SHELL_DEVTOOLS_REMOTE_PORT)
    possible_browsers.append(b)

  if 'package:' + CHROME_PACKAGE in packages:
    b = PossibleAndroidBrowser('android-chrome',
                               options, adb,
                               CHROME_PACKAGE, False,
                               CHROME_COMMAND_LINE,
                               CHROME_ACTIVITY,
                               CHROME_DEVTOOLS_REMOTE_PORT)
    possible_browsers.append(b)

  if 'package:' + CHROME_JB_SYSTEM_PACKAGE in packages:
    b = PossibleAndroidBrowser('android-jb-system-chrome',
                               options, adb,
                               CHROME_JB_SYSTEM_PACKAGE, False,
                               CHROME_COMMAND_LINE,
                               CHROME_ACTIVITY,
                               CHROME_JB_SYSTEM_DEVTOOLS_REMOTE_PORT)
    possible_browsers.append(b)

  # See if the "forwarder" is installed -- we need this to host content locally
  # but make it accessible to the device.
  if len(possible_browsers) and not adb_commands.HasForwarder(adb):
    logging.warn("chrome_remote_control detected an android device. However,")
    logging.warn("Chrome's port-forwarder app was not installed on the device.")
    logging.warn("To build:")
    logging.warn("  make -j16 out/$BUILDTYPE/forwarder")
    logging.warn("And then install it:")
    logging.warn("  %s" % adb_commands.HowToInstallForwarder())
    logging.warn("")
    logging.warn("")
    return []
  return possible_browsers

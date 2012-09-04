# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess
import logging

import chrome_remote_control.android_browser_backend as android_browser_backend
import chrome_remote_control.adb_commands as real_adb_commands
import browser
import possible_browser


"""Finds android browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = 'android-content-shell'

# Commmand line
#  content-shell: /data/local/tmp/content-shell-command-line
#            or   /data/local/tmp/chrome-command-line
#  clank:         /data/local/chrome-command-line

# --remote-debugging-port=9222
# --disable-fre

# Forwarding command:
#  adb forward tcp:xxxx localabstract:chrome_devtools-remote

# Package names:
#   org.chromium.content_shell
#   com.android.chrome # m18 accidentally used this for a while
#   com.google.android.apps.chrome

CHROME_PACKAGE = 'com.google.android.apps.chrome'
CHROME_ACTIVITY = '.Main'
CHROME_COMMAND_LINE = '/data/local/chrome-command-line'
CHROME_DEVTOOLS_REMOTE_PORT = 'localabstract:chrome_devtools-remote'

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
  browsers = []

  # See if adb even works.
  try:
    with open(real_os.devnull, 'w') as devnull:
      subprocess.call('adb', stdout=devnull, stderr=devnull)
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

  packages = adb.RunShellCommand('pm list packages')
  if 'package:' + CONTENT_SHELL_PACKAGE in packages:
    b = PossibleAndroidBrowser('android-content-shell',
                               options, adb,
                               CONTENT_SHELL_PACKAGE, True,
                               CONTENT_SHELL_COMMAND_LINE,
                               CONTENT_SHELL_ACTIVITY,
                               CONTENT_SHELL_DEVTOOLS_REMOTE_PORT)
    browsers.append(b)

  return browsers

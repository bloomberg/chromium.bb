# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os as real_os
import sys as real_sys
import subprocess as real_subprocess

import browser
import desktop_browser_backend
import possible_browser

"""Finds desktop browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = ','.join([
    'exact',
    'release',
    'debug',
    'canary',
    'content-shell-debug',
    'content-shell-release',
    'system'])

class PossibleDesktopBrowser(possible_browser.PossibleBrowser):
  """A desktop browser that can be controlled."""

  def __init__(self, type, options, executable, is_content_shell):
    super(PossibleDesktopBrowser, self).__init__(type, options)
    self._local_executable = executable
    self._is_content_shell = is_content_shell

  def __repr__(self):
    return 'PossibleDesktopBrowser(type=%s)' % self.type

  def Create(self):
    backend = desktop_browser_backend.DesktopBrowserBackend(
        self._options, self._local_executable, self._is_content_shell)
    return browser.Browser(backend)

def FindAllAvailableBrowsers(options,
                             os = real_os,
                             sys = real_sys,
                             subprocess = real_subprocess):
  """Finds all the desktop browsers available on this machine."""
  browsers = []

  has_display = True
  if (sys.platform.startswith('linux') and
      os.getenv('DISPLAY') == None):
    has_display = False

  # Add the explicit browser executable if given.
  if options.browser_executable:
    if os.path.exists(options.browser_executable):
      browsers.append(PossibleDesktopBrowser('exact', options,
                                      options.browser_executable, False))

  # Look for a browser in the standard chrome build locations.
  if options.chrome_root:
    chrome_root = options.chrome_root
  else:
    chrome_root = os.path.join(os.path.dirname(__file__), '..', '..', '..')

  if sys.platform == 'darwin':
    chromium_app_name = 'Chromium.app/Contents/MacOS/Chromium'
    content_shell_app_name = 'Content Shell.app/Contents/MacOS/Content Shell'
  elif sys.platform.startswith('linux'):
    chromium_app_name = 'chrome'
    content_shell_app_name = 'content_shell'
  elif sys.platform.startswith('win'):
    chromium_app_name = 'chrome.exe'
    content_shell_app_name = 'content_shell.exe'
  else:
    raise Exception('Platform not recognized')

  if sys.platform.startswith('win'):
    build_dir = 'build'
  else:
    build_dir = 'out'

  # Add local builds
  def AddIfFound(type, type_dir, app_name, content_shell):
    app = os.path.join(chrome_root, build_dir, type_dir, app_name)
    if os.path.exists(app):
      browsers.append(PossibleDesktopBrowser(type, options,
                                             app, content_shell))
  AddIfFound('debug', 'Debug', chromium_app_name, False)
  AddIfFound('content-shell-debug', 'Debug', content_shell_app_name, True)
  AddIfFound('release', 'Release', chromium_app_name, False)
  AddIfFound('content-shell-release', 'Release', content_shell_app_name, True)

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary = ('/Applications/Google Chrome Canary.app/'
                 'Contents/MacOS/Google Chrome Canary')
    mac_system = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
    if os.path.exists(mac_canary):
      browsers.append(PossibleDesktopBrowser('canary', options,
                                             mac_canary, False))

    if os.path.exists(mac_system):
      browsers.append(PossibleDesktopBrowser('system', options,
                                             mac_system, False))

  # Linux specific options.
  if sys.platform.startswith('linux'):
    # Look for a google-chrome instance.
    found = False
    try:
      with open(real_os.devnull, 'w') as devnull:
        found = subprocess.call(['google-chrome', '--version'],
                                stdout=devnull, stderr=devnull) == 0
    except OSError:
      pass
    if found:
      browsers.append(
          PossibleDesktopBrowser('system', options,
                                 'google-chrome', False))

  # Win32-specific options.
  if sys.platform.startswith('win') and os.getenv('LOCALAPPDATA'):
    local_app_data = os.getenv('LOCALAPPDATA')
    win_canary = os.path.join(local_app_data,
                              'Google\\Chrome SxS\\Application\\chrome.exe')
    win_system = os.path.join(local_app_data,
                              'Google\\Chrome\\Application\\chrome.exe')
    if os.path.exists(win_canary):
      browsers.append(PossibleDesktopBrowser('canary', options,
                                             win_canary, False))

    if os.path.exists(win_system):
      browsers.append(PossibleDesktopBrowser('system', options,
                                             win_system, False))

  if len(browsers) and not has_display:
    logging.warning('Found (%s), but you have a DISPLAY environment set.' %
                    ','.join([b.type for b in browsers]))
    return []

  return browsers

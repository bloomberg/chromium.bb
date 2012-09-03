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

ALL_BROWSER_TYPES = "exact,release,debug,canary,system"
DEFAULT_BROWSER_TYPES_TO_RUN = "exact,release,canary,system"

class PossibleDesktopBrowser(possible_browser.PossibleBrowser):
  """A desktop browser that can be controlled."""

  def __init__(self, type, options, executable):
    super(PossibleDesktopBrowser, self).__init__(type, options)
    self._local_executable = executable

  def __repr__(self):
    return "PossibleDesktopBrowser(type=%s)" % self.type

  def Create(self):
    backend = desktop_browser_backend.DesktopBrowserBackend(
        self._options, self._local_executable)
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
      browsers.append(PossibleDesktopBrowser("exact", options,
                                      options.browser_executable))

  # Look for a browser in the standard chrome build locations.
  if options.chrome_root:
    chrome_root = options.chrome_root
  else:
    chrome_root = os.path.join(os.path.dirname(__file__), "..", "..", "..")

  if sys.platform == 'darwin':
    app_name = "Chromium.app/Contents/MacOS/Chromium"
  elif sys.platform.startswith('linux'):
    app_name = "chrome"
  elif sys.platform.startswith('win'):
    app_name = "chrome.exe"
  else:
    raise Exception("Platform not recognized")

  if sys.platform.startswith('win'):
    build_dir = "build"
  else:
    build_dir = "out"

  debug_app = os.path.join(chrome_root, build_dir, "Debug", app_name)
  if os.path.exists(debug_app):
    browsers.append(PossibleDesktopBrowser("debug", options, debug_app))

  release_app = os.path.join(chrome_root, build_dir, "Release", app_name)
  if os.path.exists(release_app):
    browsers.append(PossibleDesktopBrowser("release", options, release_app))

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary = ("/Applications/Google Chrome Canary.app/"
                 "Contents/MacOS/Google Chrome Canary")
    mac_system = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    if os.path.exists(mac_canary):
      browsers.append(PossibleDesktopBrowser("canary", options, mac_canary))

    if os.path.exists(mac_system):
      browsers.append(PossibleDesktopBrowser("system", options, mac_system))

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
          PossibleDesktopBrowser("system", options, 'google-chrome'))

  # Win32-specific options.
  if sys.platform.startswith('win') and os.getenv('LOCALAPPDATA'):
    local_app_data = os.getenv('LOCALAPPDATA')
    win_canary = os.path.join(local_app_data,
                              'Google\\Chrome SxS\\Application\\chrome.exe')
    win_system = os.path.join(local_app_data,
                              'Google\\Chrome\\Application\\chrome.exe')
    if os.path.exists(win_canary):
      browsers.append(PossibleDesktopBrowser("canary", options, win_canary))

    if os.path.exists(win_system):
      browsers.append(PossibleDesktopBrowser("system", options, win_system))

  if len(browsers) and not has_display:
    logging.warning('Found (%s), but you have a DISPLAY environment set.' %
                    ','.join([b.type for b in browsers]))
    return []

  return browsers

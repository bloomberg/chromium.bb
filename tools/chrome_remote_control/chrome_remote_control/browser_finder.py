# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess

import browser
import desktop_browser_backend

"""Finds browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = "exact,release,debug,canary,system"
DEFAULT_BROWSER_TYPES_TO_RUN = "exact,release,canary,system"

class PossibleBrowser(object):
  """A browser that can be controlled.

  Call connect() to launch the browser and begin manipulating it..
  """

  def __init__(self, type, options, executable):
    self.type = type
    self._options = options
    self._local_executable = executable

  def __repr__(self):
    return "PossibleBrowser(type=%s)" % self.type

  def Create(self):
    if self._local_executable:
      backend = desktop_browser_backend.DesktopBrowserBackend(
          self._local_executable, self._options)
      return browser.Browser(backend)
    raise Exception("Not implemented.")

def FindBestPossibleBrowser(options,
                            os = real_os,
                            sys = real_sys,
                            subprocess = real_subprocess):
  """Finds the best PossibleBrowser object to run given the provided
  BrowserOptions object. The returned possiblity object can then be used to
  connect to and control the located browser."""

  browsers = FindAllPossibleBrowsers(options, os, sys, subprocess)
  if len(browsers):
    return browsers[0]
  return None

def GetAllAvailableBrowserTypes(options):
  """Returns an array of browser types supported on this system."""
  browsers = FindAllPossibleBrowsers(options)
  type_set = set([browser.type for browser in browsers])
  type_list = list(type_list)
  type_list.sort()
  return type_list

def FindAllPossibleBrowsers(options,
                    os = real_os,
                    sys = real_sys,
                    subprocess = real_subprocess):
  """Finds all browsers that can be created given the options. Returns an array
  of PossibleBrowser objects, sorted and filtered by
  options.browser_types_to_use."""
  browsers = _UnsortedFindAllLocalBrowserPossibilities(options,
      os, sys, subprocess)
  selected_browsers = [browser
                       for browser in browsers
                       if browser.type in options.browser_types_to_use]
  def compare_browsers_on_priority(x, y):
    x_idx = options.browser_types_to_use.index(x.type)
    y_idx = options.browser_types_to_use.index(y.type)
    return x_idx - y_idx
  selected_browsers.sort(compare_browsers_on_priority)
  return selected_browsers

def _UnsortedFindAllLocalBrowserPossibilities(options,
                                              os = real_os,
                                              sys = real_sys,
                                              subprocess = real_subprocess):
  browsers = []

  # Add the explicit browser executable if given.
  if options.browser_executable:
    if os.path.exists(options.browser_executable):
      browsers.append(PossibleBrowser("exact", options,
                                      options.browser_executable))

  # Look for a browser in the standard chrome build locations.
  if options.chrome_root:
    chrome_root = options.chrome_root
  else:
    chrome_root = os.path.join(os.path.dirname(__file__), "../../../")

  if sys.platform == 'darwin':
    app_name = "Chromium.app/Contents/MacOS/Chromium"
  elif sys.platform.startswith('linux'):
    app_name = "chrome"
  elif sys.platform == 'win':
    app_name = "chrome.exe"
  else:
    raise Exception("Platform not recognized")

  debug_app = os.path.join(chrome_root, "out", "Debug", app_name)
  if os.path.exists(debug_app):
    browsers.append(PossibleBrowser("debug", options, debug_app))

  release_app = os.path.join(chrome_root, "out", "Release", app_name)
  if os.path.exists(release_app):
    browsers.append(PossibleBrowser("release", options, release_app))

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary = ("/Applications/Google Chrome Canary.app/"
                 "Contents/MacOS/Google Chrome Canary")
    mac_stable = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    if os.path.exists(mac_canary):
      browsers.append(PossibleBrowser("canary", options, mac_canary))

    if os.path.exists(mac_stable):
      browsers.append(PossibleBrowser("system", options, mac_stable))

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
      browsers.append(PossibleBrowser("system", options, 'google-chrome'))

  return browsers

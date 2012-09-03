# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import android_browser_finder
import desktop_browser_finder

"""Finds browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = (
    desktop_browser_finder.ALL_BROWSER_TYPES + "," +
    android_browser_finder.ALL_BROWSER_TYPES)

DEFAULT_BROWSER_TYPES_TO_RUN = (
    desktop_browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN + "," +
    android_browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN)

def FindBestPossibleBrowser(options):
  """Finds the best PossibleBrowser object to run given the provided
  BrowserOptions object. The returned possiblity object can then be used to
  connect to and control the located browser."""

  browsers = FindAllPossibleBrowsers(options)
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

def FindAllPossibleBrowsers(options):
  """Finds all browsers that can be created given the options. Returns an array
  of PossibleBrowser objects, sorted and filtered by
  options.browser_types_to_use."""
  browsers = []
  browsers.extend(desktop_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(android_browser_finder.FindAllAvailableBrowsers(options))

  selected_browsers = [browser
                       for browser in browsers
                       if browser.type in options.browser_types_to_use]
  def compare_browsers_on_priority(x, y):
    x_idx = options.browser_types_to_use.index(x.type)
    y_idx = options.browser_types_to_use.index(y.type)
    return x_idx - y_idx
  selected_browsers.sort(compare_browsers_on_priority)
  return selected_browsers

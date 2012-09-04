# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

import android_browser_finder
import desktop_browser_finder

"""Finds browsers that can be controlled by chrome_remote_control."""

ALL_BROWSER_TYPES = (
    desktop_browser_finder.ALL_BROWSER_TYPES + "," +
    android_browser_finder.ALL_BROWSER_TYPES)

class BrowserTypeRequiredException(Exception):
  pass

def FindBrowser(options):
  """Finds the best PossibleBrowser object to run given the provided
  BrowserOptions object. The returned possiblity object can then be used to
  connect to and control the located browser."""
  if options.browser_type == 'exact' and options.browser_executable == None:
    raise Exception("browser_type=exact requires browser_executable be set.")

  if options.browser_type != 'exact' and options.browser_executable != None:
    raise Exception("browser_executable requires browser_executable=exact.")

  if options.browser_type == None:
    raise BrowserTypeRequiredException("browser_type must be specified")

  browsers = []
  browsers.extend(desktop_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(android_browser_finder.FindAllAvailableBrowsers(options))

  if options.browser_type == 'any':
    if len(browsers) >= 1:
      return browsers[0]
    else:
      return None

  matching_browsers = [b for b in browsers if b.type == options.browser_type]

  if len(matching_browsers) == 1:
    return matching_browsers[0]
  elif len(matching_browsers) > 1:
    logging.warning('Multiple browsers of the same type found: %s' % (
                    repr(matching_browsers)))
    return matching_browsers[0]
  else:
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
  selected_browsers = [browser
                       for browser in browsers
                       if browser.type in options.browser_types_to_use]
  def compare_browsers_on_priority(x, y):
    x_idx = options.browser_types_to_use.index(x.type)
    y_idx = options.browser_types_to_use.index(y.type)
    return x_idx - y_idx
  selected_browsers.sort(compare_browsers_on_priority)
  return selected_browsers

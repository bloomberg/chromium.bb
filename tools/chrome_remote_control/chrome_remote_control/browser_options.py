# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse

import browser_finder

class BrowserOptions(object):
  @staticmethod
  def CreateParser(usage=None):
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('--dont-override-profile', action="store_true",
        dest="dont_override_profile",
        help="Uses the regular user profile instead of a clean one")
    parser.add_option('--browser-executable', action="store_true",
        dest="browser_executable",
        help="The exact browser to run.")
    parser.add_option('--browser-types-to-use', action="store_true",
        dest="browser_types_to_use",
        help="Comma-separated list of browsers to run, "
             "in order of priority. Possible values: %s" %
             browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN)
    parser.add_option('--chrome-root', action="store_true",
        dest="chrome_root",
        help="Where to look for chrome builds."
             "Defaults to searching parent dirs by default.")
    real_parse = parser.parse_args
    def ParseArgs(args = None):
      options = BrowserOptions()
      ret = real_parse(args, options)
      return ret
    parser.parse_args = ParseArgs
    return parser

  def __init__(self):
    self.dont_override_profile = False
    self.hide_stdout = True
    self.browser_executable = None
    self._browser_types_to_use =  (
        browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN.split(","))
    self.chrome_root = None
    self.extra_browser_args = []

  @property
  def browser_types_to_use(self):
    return self._browser_types_to_use

  @browser_types_to_use.setter
  def browser_types_to_use(self, value):
    self._browser_types_to_use.split(",")


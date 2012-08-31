# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse

ALL_BROWSER_TYPES = 'exact,release,debug,canary,system'
DEFAULT_BROWSER_TYPES_TO_RUN = 'exact,release,canary,system'

class BrowserOptions(object):
  @staticmethod
  def CreateParser(usage=None):
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('--dont-override-profile', action='store_true',
        dest='dont_override_profile',
        help='Uses the regular user profile instead of a clean one')
    parser.add_option('--browser-executable', dest='browser_executable',
        help='The exact browser to run.')
    parser.add_option('--browser-types-to-use', dest='browser_types_to_use',
        help='Comma-separated list of browsers to run, '
             'in order of priority. Possible values: %s' %
             DEFAULT_BROWSER_TYPES_TO_RUN)
    parser.add_option('--chrome-root', dest='chrome_root',
        help='Where to look for chrome builds. '
             'Defaults to searching parent dirs.')
    real_parse = parser.parse_args
    def ParseArgs(args=None):
      options = BrowserOptions()
      ret = real_parse(args, options)
      return ret
    parser.parse_args = ParseArgs
    return parser

  def __init__(self):
    self.dont_override_profile = False
    self.hide_stdout = True
    self.browser_executable = None
    self.browser_types_to_use = DEFAULT_BROWSER_TYPES_TO_RUN
    self.chrome_root = None
    self.extra_browser_args = []

  @property
  def browser_types_to_use(self):
    return self._browser_types_to_use

  @browser_types_to_use.setter
  def browser_types_to_use(self, value):
    self._browser_types_to_use = value.split(',')


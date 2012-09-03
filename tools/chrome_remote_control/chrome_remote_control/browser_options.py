# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import browser_finder

class BrowserOptions(object):
  """Options to be used for disocvering and launching browsers."""

  def __init__(self):
    self.dont_override_profile = False
    self.hide_stdout = True
    self.browser_executable = None
    self._browser_types_to_use =  (
        browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN.split(","))
    self.chrome_root = None
    self.android_device = None
    self.extra_browser_args = []

  def CreateParser(self, *args, **kwargs):
    parser = optparse.OptionParser(*args, **kwargs)
    parser.add_option('--dont-override-profile', action='store_true',
        dest='dont_override_profile',
        help='Uses the regular user profile instead of a clean one')
    parser.add_option('--browser-executable',
        dest='browser_executable',
        help='The exact browser to run.')
    parser.add_option('--browser-types-to-use',
        dest='browser_types_to_use',
        help='Comma-separated list of browsers to run, '
             'in order of priority. Possible values: %s' %
             browser_finder.DEFAULT_BROWSER_TYPES_TO_RUN)
    parser.add_option('--chrome-root',
        dest='chrome_root',
        help='Where to look for chrome builds.'
             'Defaults to searching parent dirs by default.')
    parser.add_option('--device',
        dest='android_device',
        help='The android device ID to use'
             'If not specified, only 0 or 1 connected devcies are supported.')
    real_parse = parser.parse_args
    def ParseArgs(args=None):
      ret = real_parse(args, self)
      return ret
    parser.parse_args = ParseArgs
    return parser

  def __getattr__(self, name):
    return None

  def __setattr__(self, name, value):
    object.__setattr__(self, name, value)
    return value

  @property
  def browser_types_to_use(self):
    return self._browser_types_to_use

  @browser_types_to_use.setter
  def browser_types_to_use(self, value):
    self._browser_types_to_use = value.split(',')

"""
This global variable can be set to a BrowserOptions object by the test harness
to allow multiple unit tests to use a specific browser, in face of multiple
options.
"""
options_for_unittests = None

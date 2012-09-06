# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import sys
import shlex

import browser_finder

class BrowserOptions(optparse.Values):
  """Options to be used for disocvering and launching browsers."""

  def __init__(self):
    optparse.Values.__init__(self)
    self.dont_override_profile = False
    self.show_stdout = False
    self.browser_executable = None
    self._browser_type =  None
    self.chrome_root = None
    self.android_device = None
    self.extra_browser_args = []

  def CreateParser(self, *args, **kwargs):
    parser = optparse.OptionParser(*args, **kwargs)
    parser.add_option('--browser',
        dest='browser_type',
        default=None,
        help='Browser type to run, '
             'in order of priority. Supported values: list,%s' %
             browser_finder.ALL_BROWSER_TYPES)
    parser.add_option('--dont-override-profile', action='store_true',
        dest='dont_override_profile',
        help='Uses the regular user profile instead of a clean one')
    parser.add_option('--browser-executable',
        dest='browser_executable',
        help='The exact browser to run.')
    parser.add_option('--chrome-root',
        dest='chrome_root',
        help='Where to look for chrome builds.'
             'Defaults to searching parent dirs by default.')
    parser.add_option('--extra-browser-args',
        dest='extra_browser_args_as_string',
        help='Additional arguments to pass to the browser when it starts')
    parser.add_option('--show-stdout',
        action='store_true',
        help="When possible, will display the stdout of the process")
    parser.add_option('--device',
        dest='android_device',
        help='The android device ID to use'
             'If not specified, only 0 or 1 connected devcies are supported.')
    real_parse = parser.parse_args
    def ParseArgs(args=None):
      defaults = parser.get_default_values()
      for k, v in defaults.__dict__.items():
        if k in self.__dict__:
          continue
        self.__dict__[k] = v
      ret = real_parse(args, self)
      if self.browser_executable and not self.browser_type:
        self.browser_type = 'exact'
      if not self.browser_executable and not self.browser_type:
        sys.stderr.write("Must provide --browser=<type>\n")
        sys.exit(1)
      if self.browser_type == 'list':
        import browser_finder
        types = browser_finder.GetAllAvailableBrowserTypes(self)
        sys.stderr.write("Available browsers:\n");
        sys.stdout.write("  %s\n" % "\n  ".join(types))
        sys.exit(1)
      if self.extra_browser_args_as_string:
        tmp = shlex.split(self.extra_browser_args_as_string)
        self.extra_browser_args.extend(tmp)
        delattr(self, 'extra_browser_args_as_string')
      return ret
    parser.parse_args = ParseArgs
    return parser

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

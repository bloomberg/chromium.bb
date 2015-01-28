#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import sys
import webbrowser

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir,
                             'build', 'android'))

from profile_chrome import chrome_startup_controller
from profile_chrome import controllers
from profile_chrome import flags
from profile_chrome import profiler
from profile_chrome import systrace_controller
from profile_chrome import ui
from pylib import android_commands
from pylib.device import device_utils


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from Android browsers startup, combined with '
                                 'Android systrace. See http://dev.chromium.org'
                                 '/developers/how-tos/trace-event-profiling-'
                                 'tool for detailed instructions for '
                                 'profiling.')
  parser.add_option('--url', help='URL to visit on startup. Default: '
                    'https://www.google.com', default='https://www.google.com',
                    metavar='URL')
  parser.add_option('--cold', help='Flush the OS page cache before starting the'
                    ' browser. Note that this require a device with root '
                    'access.', default=False, action='store_true')
  parser.add_option_group(flags.SystraceOptions(parser))
  parser.add_option_group(flags.OutputOptions(parser))

  browsers = sorted(profiler.GetSupportedBrowsers().keys())
  parser.add_option('-b', '--browser', help='Select among installed browsers. '
                    'One of ' + ', '.join(browsers) + ', "stable" is used by '
                    'default.', type='choice', choices=browsers,
                    default='stable')
  parser.add_option('-v', '--verbose', help='Verbose logging.',
                    action='store_true')
  parser.add_option('-z', '--compress', help='Compress the resulting trace '
                    'with gzip. ', action='store_true')
  return parser


def main():
  parser = _CreateOptionParser()
  options, _ = parser.parse_args()

  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)

  devices = android_commands.GetAttachedDevices()
  if len(devices) != 1:
    logging.error('Exactly 1 device must be attached.')
    return 1
  device = device_utils.DeviceUtils(devices[0])
  package_info = profiler.GetSupportedBrowsers()[options.browser]

  if options.systrace_categories in ['list', 'help']:
    ui.PrintMessage('\n'.join(
        systrace_controller.SystraceController.GetCategories(device)))
    return 0
  systrace_categories = (options.systrace_categories.split(',')
                         if options.systrace_categories else [])
  enabled_controllers = []
  # Enable the systrace and chrome controller. The systrace controller should go
  # first because otherwise the resulting traces miss early systrace data.
  if systrace_categories:
    enabled_controllers.append(systrace_controller.SystraceController(
        device, systrace_categories, False))
  enabled_controllers.append(
      chrome_startup_controller.ChromeStartupTracingController(
          device, package_info, options.cold, options.url))
  if options.output:
    options.output = os.path.expanduser(options.output)
  result = profiler.CaptureProfile(enabled_controllers, 0,
                                   output=options.output,
                                   compress=options.compress,
                                   write_json=options.json)
  if options.view:
    if sys.platform == 'darwin':
      os.system('/usr/bin/open %s' % os.path.abspath(result))
    else:
      webbrowser.open(result)


if __name__ == '__main__':
  sys.exit(main())

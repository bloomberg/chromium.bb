#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, 'telemetry'))

from telemetry.core import browser_finder
from telemetry.core import browser_options


def _RunPrebuilt(options):
  browser_to_create = browser_finder.FindBrowser(options)
  with browser_to_create.Create() as browser:
    browser.Start()
    output_file = os.path.join(tempfile.mkdtemp(), options.profiler)
    raw_input('Press enter to start profiling...')
    print '>> Starting profiler', options.profiler
    browser.StartProfiling(options.profiler, output_file)
    print 'Press enter or CTRL+C to stop'
    try:
      raw_input()
    except KeyboardInterrupt:
      pass
    finally:
      browser.StopProfiling()
    print '<< Stopped profiler ', options.profiler


if __name__ == '__main__':
  browser_finder_options = browser_options.BrowserFinderOptions()
  parser = browser_finder_options.CreateParser('')
  profiler_options, _ = parser.parse_args()
  sys.exit(_RunPrebuilt(profiler_options))

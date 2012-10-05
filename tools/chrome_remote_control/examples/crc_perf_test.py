#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import time

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import chrome_remote_control

def Main(args):
  options = chrome_remote_control.BrowserOptions()
  parser = options.CreateParser('crc_perf_test.py')
  options, args = parser.parse_args(args)

  browser_to_create = chrome_remote_control.FindBrowser(options)
  assert browser_to_create
  with browser_to_create.Create() as b:
    with b.ConnectToNthTab(0) as tab:

      # Measure round-trip-time for evaluate
      times = []
      for i in range(1000):
        start = time.time()
        tab.runtime.Evaluate('%i * 2' % i)
        times.append(time.time() - start)
      avg = sum(times, 0.0) / float(len(times))
      squared_diffs = [(t - avg) * (t - avg) for t in times]
      stdev = sum(squared_diffs, 0.0) / float(len(times) - 1)

      print "Average evaluate round-trip time: %f +/- %f (seconds)" % (
        avg, stdev)

  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

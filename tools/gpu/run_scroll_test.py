#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The test takes a list of URLs through stdin and prints results in CSV format.
# Example: python run_scroll_test.py < data/urls.txt > test_results.csv
import csv
import logging
import sys

import pylib.scroll
import pylib.url_test

def Main():
  results_writer = csv.writer(sys.stdout)
  results_writer.writerow(['URL', 'FPS', 'Mean Frame Time (s)',
                           '% Dropped Frames', 'First Paint Time (s)'])
  sys.stdout.flush()

  for result in pylib.url_test.UrlTest(pylib.scroll.Scroll, sys.stdin):
    if not result.DidScroll():
      # Most likely the page was shorter than the window height.
      logging.warning('Page did not scroll: %s', result.GetUrl())
      continue

    results_writer.writerow([result.GetUrl(), result.GetFps(0),
                             result.GetMeanFrameTime(0),
                             result.GetPercentBelow60Fps(0),
                             result.GetFirstPaintTime()])
    sys.stdout.flush()

if __name__ == '__main__':
  Main()

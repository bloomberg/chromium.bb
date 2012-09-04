# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The test takes a list of URLs through stdin and prints results in CSV format.
# Example: python run_scroll_test.py < data/urls.txt > test_results.csv
import csv
import logging
import os
import sys

import chrome_remote_control

import scroll
import page_set
import url_test


def Main(args):
  options = chrome_remote_control.BrowserOptions()
  parser = options.CreateParser("page_scroller <pageset>")
  _, args = parser.parse_args(args)
  if len(args) != 1:
    parser.print_usage()
    import page_sets
    sys.stderr.write("Available pagesets:\n%s\n\n" % ",\n".join(
        [os.path.relpath(f) for f in page_sets.GetAllPageSetFilenames()]))
    sys.exit(1)

  ps = page_set.PageSet()
  ps.LoadFromFile(args[0])
  return Run(sys.stdout, options, ps)

def Run(output_stream, options, ps):
  results_writer = csv.writer(output_stream)
  results_writer.writerow(['URL', 'FPS', 'Mean Frame Time (s)',
                           '% Dropped Frames', 'First Paint Time (s)'])
  possible_browser = chrome_remote_control.FindBrowser(options)
  with possible_browser.Create() as b:
    for result in url_test.UrlTest(b, scroll.Scroll, ps):
      if not result.DidScroll():
        # Most likely the page was shorter than the window height.
        logging.warning('Page did not scroll: %s', result.GetUrl())
        continue

      results_writer.writerow([result.GetUrl(), result.GetFps(0),
                               result.GetMeanFrameTime(0),
                               result.GetPercentBelow60Fps(0),
                               result.GetFirstPaintTime()])
      sys.stdout.flush()

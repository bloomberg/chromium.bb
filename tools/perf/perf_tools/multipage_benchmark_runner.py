#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import csv
import logging
import os
import sys

from chrome_remote_control import browser_finder
from chrome_remote_control import browser_options
from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import page_runner
from chrome_remote_control import page_set

import perf_tools.first_paint_time_benchmark
import perf_tools.scrolling_benchmark
import perf_tools.skpicture_printer
import perf_tools.texture_upload_benchmark

# TODO(tonyg/nduca): Discover benchmarks automagically.
_BENCHMARKS = {
  'first_paint_time_benchmark':
      perf_tools.first_paint_time_benchmark.FirstPaintTimeBenchmark,
  'scrolling_benchmark':
      perf_tools.scrolling_benchmark.ScrollingBenchmark,
  'skpicture_printer':
      perf_tools.skpicture_printer.SkPicturePrinter,
  'texture_upload_benchmark':
      perf_tools.texture_upload_benchmark.TextureUploadBenchmark
}


def Main():
  """Turns a MultiPageBenchmark into a command-line program.

  If args is not specified, sys.argv[1:] is used.
  """

  # Naively find the benchmark. If we use the browser options parser, we run
  # the risk of failing to parse if we use a benchmark-specific parameter.
  benchmark_name = None
  for arg in sys.argv:
    if arg in _BENCHMARKS:
      benchmark_name = arg

  options = browser_options.BrowserOptions()
  parser = options.CreateParser('%prog [options] <benchmark> <page_set>')

  benchmark = None
  if benchmark_name is not None:
    benchmark = _BENCHMARKS[benchmark_name]()
    benchmark.AddOptions(parser)

  _, args = parser.parse_args()

  if benchmark is None or len(args) != 2:
    parser.print_usage()
    import page_sets # pylint: disable=F0401
    print >> sys.stderr, 'Available benchmarks:\n%s\n' % ',\n'.join(
        _BENCHMARKS.keys())
    print >> sys.stderr, 'Available page_sets:\n%s\n' % ',\n'.join(
        [os.path.relpath(f) for f in page_sets.GetAllPageSetFilenames()])
    sys.exit(1)

  ps = page_set.PageSet.FromFile(args[1])

  benchmark.CustomizeBrowserOptions(options)
  possible_browser = browser_finder.FindBrowser(options)
  if not possible_browser:
    print >> sys.stderr, """No browser found.\n
Use --browser=list to figure out which are available.\n"""
    sys.exit(1)

  results = multi_page_benchmark.CsvBenchmarkResults(csv.writer(sys.stdout))
  with page_runner.PageRunner(ps) as runner:
    runner.Run(options, possible_browser, benchmark, results)
  # When using an exact executable, assume it is a reference build for the
  # purpose of outputting the perf results.
  results.PrintSummary(options.browser_executable and '_ref' or '')

  if len(results.page_failures):
    logging.warning('Failed pages: %s', '\n'.join(
        [failure['page'].url for failure in results.page_failures]))
  return min(255, len(results.page_failures))

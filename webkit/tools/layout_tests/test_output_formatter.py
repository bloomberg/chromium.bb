#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This is a script for generating easily-viewable comparisons of text and pixel
diffs.
"""
import optparse

from layout_package import test_expectations
from layout_package import failure
from layout_package import failure_finder
from layout_package import failure_finder_test
from layout_package import html_generator

DEFAULT_BUILDER = "Webkit"

def main(options, args):

  if options.run_tests:
    fft = failure_finder_test.FailureFinderTest()
    return fft.runTests()

  # TODO(gwilson): Add a check that verifies the given platform exists.

  finder = failure_finder.FailureFinder(options.build_number,
                                        options.platform_builder,
                                        (not options.include_expected),
                                        options.test_regex,
                                        options.output_dir,
                                        int(options.max_failures),
                                        options.verbose)
  failure_list = finder.GetFailures()

  if not failure_list:
    print "Did not find any failures."
    return

  generator = html_generator.HTMLGenerator(failure_list,
                                           options.output_dir,
                                           finder.build,
                                           options.platform_builder,
                                           (not options.include_expected))
  filename = generator.GenerateHTML()

  if filename and options.verbose:
    print "File created at %s" % filename

if __name__ == "__main__":
  option_parser = optparse.OptionParser()
  option_parser.add_option("-v", "--verbose", action = "store_true",
                           default = False,
                           help = "Display lots of output.")
  option_parser.add_option("-i", "--include-expected", action = "store_true",
                           default = False,
                           help = "Include expected failures in output")
  option_parser.add_option("-p", "--platform-builder",
                           default = DEFAULT_BUILDER,
                           help = "Use the given builder")
  option_parser.add_option("-b", "--build-number",
                           default = None,
                           help = "Use the given build number")
  option_parser.add_option("-t", "--test-regex",
                           default = None,
                           help = "Use the given regex to filter tests")
  option_parser.add_option("-o", "--output-dir",
                           default = ".",
                           help = "Output files to given directory")
  option_parser.add_option("-m", "--max-failures",
                           default = 100,
                           help = "Limit the maximum number of failures")
  option_parser.add_option("-r", "--run-tests", action = "store_true",
                           default = False,
                           help = "Runs unit tests")
  options, args = option_parser.parse_args()
  main(options, args)


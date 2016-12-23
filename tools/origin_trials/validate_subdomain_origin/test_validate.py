#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the validate_subdomain_origin utility

usage: test_validate.py [-h] [--utility-path UTILITY_PATH]

"""
import argparse
import os
import subprocess
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))

STATUS_VALID = 0
STATUS_INVALID_ORIGIN = 1
STATUS_IN_PUBLIC_SUFFIX_LIST = 2
STATUS_ERROR = 3
STATUS_IP_ADDRESS = 4

TestOrigins = {
  'https//': STATUS_INVALID_ORIGIN,
  'https://example.com:xx': STATUS_INVALID_ORIGIN,
  'https://google.com': STATUS_VALID,
  'google.com': STATUS_VALID,
  'http://10.0.0.1': STATUS_IP_ADDRESS,
  '10.0.0.1': STATUS_IP_ADDRESS,
  'https://com': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'https://com:443': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'com': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'co.uk': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'github.io': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'githubusercontent.com': STATUS_IN_PUBLIC_SUFFIX_LIST,
  'https://adsf': STATUS_VALID,
  'adsf': STATUS_VALID,
}

# Default utility path, relative to script_dir.
#  - Assumes utility is compiled to standard output directory, e.g.
#    <chromium dir>/src/out/Default
DEFAULT_UTILITY_PATH = '../../../out/Default/validate_subdomain_origin'

def main():
  default_utility_path = os.path.join(script_dir, DEFAULT_UTILITY_PATH)

  parser = argparse.ArgumentParser(
      description="Test the validate_subdomain_origin utility")
  parser.add_argument("--utility-path",
                      help="Path to the compiled utility",
                      default=default_utility_path)

  args = parser.parse_args()

  utility_path = os.path.expanduser(args.utility_path)
  if not os.path.exists(utility_path):
    print "ERROR"
    print "Utility not found at: %s" % utility_path
    print
    sys.exit(1)

  print "Using compiled utility found at: %s" % utility_path
  print

  failed_tests = 0

  # Test handling of number of arguments
  no_args_rc = subprocess.call(utility_path)
  if no_args_rc != STATUS_ERROR:
    failed_tests += 1
    print "Test failed for no arguments: expected %d, actual %d" % (
          STATUS_ERROR, no_args_rc)

  too_many_args_rc = subprocess.call([utility_path, "first", "second"])
  if too_many_args_rc != STATUS_ERROR:
    failed_tests += 1
    print "Test failed for 2 arguments: expected %d, actual %d" % (
          STATUS_ERROR, too_many_args_rc)

  # Test validation of various origins, and formats
  for origin, expected_result in TestOrigins.items():
    rc = subprocess.call([utility_path, origin])
    if rc != expected_result:
      failed_tests += 1
      print "Test failed for '%s': expected %d, actual %d" % (
            origin, expected_result, rc)
      print
      continue

  if failed_tests > 0:
    print "Failed %d tests" % failed_tests
    print
    sys.exit(1)

  print "All tests passed"

if __name__ == "__main__":
  main()

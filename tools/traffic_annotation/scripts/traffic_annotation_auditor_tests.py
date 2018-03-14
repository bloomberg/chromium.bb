#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs tests to ensure annotation tests are working as expected.
"""

import os
import argparse
import sys

from annotation_tools import NetworkTrafficAnnotationTools

# If this test starts failing, please set TEST_IS_ENABLED to "False" and file a
# bug to get this reenabled, and cc the people listed in
# //tools/traffic_annotation/OWNERS.
TEST_IS_ENABLED = False


class TrafficAnnotationTestsChecker():
  def __init__(self, build_path=None):
    """Initializes a TrafficAnnotationTestsChecker object.

    Args:
      build_path: str Absolute or relative path to a fully compiled build
          directory.
    """
    self.tools = NetworkTrafficAnnotationTools(build_path)

  def RunAllTests(self):
    result = self.RunOnAllFiles()
    #TODO(rhalavati): Add more tests, and create a pipeline for them.
    return result

  def RunOnAllFiles(self):
    args = ["--test-only"]
    _, stderr_text, return_code = self.tools.RunAuditor(args)

    if not return_code:
      print("RunOnAlFiles Passed.")
    elif stderr_text:
      print(stderr_text)
    return return_code


def main():
  if not TEST_IS_ENABLED:
    return 0

  parser = argparse.ArgumentParser(
      description="Traffic Annotation Tests checker.")
  parser.add_argument(
      '--build-path',
      help='Specifies a compiled build directory, e.g. out/Debug. If not '
           'specified, the script tries to guess it. Will not proceed if not '
           'found.')

  args = parser.parse_args()
  checker = TrafficAnnotationTestsChecker(args.build_path)
  return checker.RunAllTests()


if '__main__' == __name__:
  sys.exit(main())
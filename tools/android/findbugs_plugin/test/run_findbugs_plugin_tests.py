#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is used to test the findbugs plugin, it calls
# build/android/pylib/findbugs.py to analyze the classes in
# org.chromium.tools.findbugs.plugin package, and expects to get the same
# issue with those in expected_result.txt.
#
# Useful command line:
# --rebaseline to generate the expected_result.txt, please make sure don't
# remove the expected result of exsting tests.


import optparse
import os
import sys

lib_folder = os.path.join(os.getenv('CHROME_SRC'), 'build', 'android', 'pylib')
if lib_folder not in sys.path:
  sys.path.append(lib_folder)

import findbugs


def main(argv):
  if not findbugs.CheckEnvironment():
    return 1

  parser = findbugs.GetCommonParser()

  options, _ = parser.parse_args()

  chrome_src = os.getenv('CHROME_SRC')

  if not options.known_bugs:
    options.known_bugs = os.path.join(chrome_src, 'tools', 'android',
                                      'findbugs_plugin', 'test',
                                      'expected_result.txt')
  if not options.only_analyze:
    options.only_analyze = 'org.chromium.tools.findbugs.plugin.*'

  return findbugs.Run(options)

if __name__ == '__main__':
  sys.exit(main(sys.argv))

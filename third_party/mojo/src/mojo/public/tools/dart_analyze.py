#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# To integrate dartanalyze with out build system, we take an input file, run
# the analyzer on it, and write a stamp file if it passed.
#
# The first argument to this script is a reference to this build's gen
# directory, which we treat as the package root. The second is the stamp file
# to touch if we succeed. The rest are passed to the analyzer verbatim.

import os
import subprocess
import sys
import re

_ANALYZING_PATTERN = re.compile(r'^Analyzing \[')
_FINAL_REPORT_PATTERN = re.compile(r'^[0-9]+ errors and [0-9]+ warnings found.')

_NATIVE_ERROR_PATTERN = re.compile(
  r'^\[error\] Native functions can only be declared in the SDK and code that '
  r'is loaded through native extensions')
_WARNING_PATTERN = re.compile(r'^\[warning\]')
_THAT_ONE_BROKEN_CLOSE_IN_WEB_SOCKETS_PATTERN = re.compile(
  r'^\[error\] The name \'close\' is already defined')

def main(args):
  cmd = [
    "../../third_party/dart-sdk/dart-sdk/bin/dartanalyzer",
  ]

  gen_dir = args.pop(0)
  stamp_file = args.pop(0)
  cmd.extend(args)
  cmd.append("--package-root=%s" % gen_dir)

  passed = True
  try:
    subprocess.check_output(cmd, shell=False, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    # Perform post processing on the output. Filter out non-error messages and
    # known problem patterns that we're working on.
    raw_lines = e.output.split('\n')
    # Remove the last empty line
    raw_lines.pop()
    filtered_lines = [i for i in raw_lines if (
      not re.match(_ANALYZING_PATTERN, i) and
      not re.match(_FINAL_REPORT_PATTERN, i) and
      # TODO(erg): Remove the rest of these as fixes land:
      not re.match(_WARNING_PATTERN, i) and
      not re.match(_NATIVE_ERROR_PATTERN, i) and
      not re.match(_THAT_ONE_BROKEN_CLOSE_IN_WEB_SOCKETS_PATTERN, i))]
    for line in filtered_lines:
      passed = False
      print >> sys.stderr, line

  if passed:
    # We passed cleanly, so touch the stamp file so that we don't run again.
    with open(stamp_file, 'a'):
      os.utime(stamp_file, None)
    return 0
  else:
    return -2

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

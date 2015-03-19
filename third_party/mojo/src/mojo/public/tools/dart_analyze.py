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

import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

_ANALYZING_PATTERN = re.compile(r'^Analyzing \[')
_ERRORS_AND_WARNINGS_PATTERN = re.compile(
  r'^[0-9]+ errors? and [0-9]+ warnings? found.')
_ERRORS_PATTERN = re.compile(r'^([0-9]+|No) (error|warning|issue)s? found.')


def main(args):
  dartzip_file = args.pop(0)
  stamp_file = args.pop(0)

  dartzip_basename = os.path.basename(dartzip_file) + ":"

  # Unzip |dartzip_file| to a temporary directory.
  try:
    temp_dir = tempfile.mkdtemp()
    zipfile.ZipFile(dartzip_file).extractall(temp_dir)

    cmd = [
      "../../third_party/dart-sdk/dart-sdk/bin/dartanalyzer",
    ]

    # Grab all the toplevel dart files in the archive.
    dart_files = glob.glob(os.path.join(temp_dir, "*.dart"))

    cmd.extend(dart_files)
    cmd.extend(args)
    cmd.append("--package-root=%s" % temp_dir)
    cmd.append("--fatal-warnings")

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
        not re.match(_ERRORS_AND_WARNINGS_PATTERN, i) and
        not re.match(_ERRORS_PATTERN, i))]
      for line in filtered_lines:
        passed = False
        print >> sys.stderr, line.replace(temp_dir + "/", dartzip_basename)

    if passed:
      # We passed cleanly, so touch the stamp file so that we don't run again.
      with open(stamp_file, 'a'):
        os.utime(stamp_file, None)
      return 0
    else:
      return -2
  finally:
    shutil.rmtree(temp_dir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

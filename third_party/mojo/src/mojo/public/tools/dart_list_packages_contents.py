#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script outputs the filenames of the files that are in the "packages/"
subdir of the given directory, relative to that directory."""

import argparse
import os
import sys

def main(target_directory):
  os.chdir(target_directory)
  for root, _, files in os.walk("packages", followlinks=True):
    for f in files:
      print os.path.join(root, f)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description="List filenames of files in the packages/ subdir of the "
                  "given directory.")
  parser.add_argument("--target-directory",
                      dest="target_directory",
                      metavar="<target-directory>",
                      type=str,
                      required=True,
                      help="The target directory, specified relative to this "
                           "directory.")
  args = parser.parse_args()
  sys.exit(main(args.target_directory))

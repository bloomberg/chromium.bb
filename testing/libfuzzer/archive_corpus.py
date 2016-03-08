#!/usr/bin/python2
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archive corpus file into zip and generate .d depfile.

Invoked by GN from fuzzer_test.gni.
"""

from __future__ import print_function
import argparse
import os
import sys
import zipfile


def main():
  parser = argparse.ArgumentParser(description="Generate fuzzer config.")
  parser.add_argument('--depfile', required=True)
  parser.add_argument('--corpus', required=True)
  parser.add_argument('--output', required=True)
  parser.add_argument('--fuzzer', required=True)
  args = parser.parse_args()

  corpus_files = []
  # Generate .d file with dependency from corpus archive to individual files.
  with open(args.depfile, 'w') as depfile:
      print(os.path.basename(args.output), ":", end="", file=depfile)
      for (dirpath, _, filenames) in os.walk(args.corpus):
          for filename in filenames:
              full_filename = os.path.join(dirpath, filename)
              print(" ", full_filename, end="", file=depfile)
              corpus_files.append(full_filename)
      # chrome bots complain about this one:
      # print(" ", args.fuzzer, end="", file=depfile)

  with zipfile.ZipFile(args.output, 'w') as z:
    for corpus_file in corpus_files:
        z.write(corpus_file, os.path.basename(corpus_file))


if __name__ == '__main__':
  main()


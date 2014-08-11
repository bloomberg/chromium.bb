#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import processor


def GetInputs(args):
  parser = argparse.ArgumentParser()
  parser.add_argument("sources", nargs=argparse.ONE_OR_MORE)
  parser.add_argument("-d", "--depends", nargs=argparse.ZERO_OR_MORE,
                      default=[])
  parser.add_argument("-e", "--externs", nargs=argparse.ZERO_OR_MORE,
                      default=[])
  opts = parser.parse_args(args)

  files = set()
  for file in opts.sources + opts.depends + opts.externs:
    files.add(file)
    files.update(processor.Processor(file).included_files())

  return files


if __name__ == "__main__":
  print "\n".join(GetInputs(sys.argv[1:]))

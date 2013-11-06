#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Checks all the expected files are mapped."""

import os
import sys

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
  expected = sorted([
    'repeated_files.py',
    'file1.txt',
    'file1_copy.txt',
  ])
  actual = sorted(os.listdir(ROOT_DIR))
  if expected != actual:
    print >> sys.stderr, 'Expected list doesn\'t match:'
    print >> sys.stderr, ', '.join(expected)
    print >> sys.stderr, ', '.join(actual)
    return 1

  print 'Success'
  return 0


if __name__ == '__main__':
  sys.exit(main())

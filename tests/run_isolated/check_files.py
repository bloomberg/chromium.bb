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
    'check_files.py',
    'file1.txt',
    'file1_copy.txt',
    'file2.txt',
    'repeated_files.py',
  ])
  actual = sorted(os.listdir(ROOT_DIR))
  if expected != actual:
    print >> sys.stderr, 'Expected list doesn\'t match:'
    print >> sys.stderr, ', '.join(expected)
    print >> sys.stderr, ', '.join(actual)
    return 1

  # Check that file2.txt is in reality file3.txt.
  with open(os.path.join(ROOT_DIR, 'file2.txt'), 'rb') as f:
    if f.read() != 'File3\n':
      print >> sys.stderr, 'file2.txt should be file3.txt in reality'
      return 2

  print 'Success'
  return 0


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time


def main():
  print 'child2'
  # Introduce a race condition with the parent so the parent may have a chance
  # to exit before the child. Will be random.
  time.sleep(.01)
  # Do not do anything with it.
  open('test_file.txt')
  # Check for case-insensitive file system. This happens on Windows and OSX.
  try:
    open('Test_File.txt')
  except OSError:
    # Pass on other OSes since this code assumes only win32 has case-insensitive
    # path. This is not strictly true.
    if sys.platform not in ('darwin', 'win32'):
      raise


if __name__ == '__main__':
  sys.exit(main())

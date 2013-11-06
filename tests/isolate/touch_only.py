#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import sys


def main():
  print 'touch_only: verify the file is empty.'
  assert len(sys.argv) == 2
  mode = sys.argv[1]
  assert mode in ('run', 'trace')

  size = os.stat(os.path.join('files1', 'test_file1.txt')).st_size
  if mode == 'run':
    # The file must be empty.
    if size:
      print 'Unexpected content'
      return 1
  else:
    # The file must be non-empty.
    if not size:
      print 'Unexpected emptiness'
      return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import sys


def main():
  print 'symlink: touches link_outside_build_root/'
  assert len(sys.argv) == 1

  if 'asdf\n' != open(
      os.path.join('link_outside_build_root', 'test_file3.txt'), 'rb').read():
    print 'Failed'
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())

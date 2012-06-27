#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  print 'symlink: touches files2/'
  assert len(sys.argv) == 1

  expected = {
    'test_file1.txt': 'Foo\n',
    'test_file2.txt': 'Bar\n',
  }

  root = 'files2'
  actual = dict(
      (filename, open(os.path.join(root, filename), 'rb').read())
      for filename in (os.listdir(root))
      if os.path.isfile(os.path.join(root, filename)))

  if actual != expected:
    print 'Failure'
    print actual
    print expected
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import sys


def main():
  print 'symlink: touches files2/'
  assert len(sys.argv) == 1

  expected = {
    os.path.join('subdir', '42.txt'):
        'the answer to life the universe and everything\n',
    'test_file1.txt': 'Foo\n',
    'test_file2.txt': 'Bar\n',
  }

  root = 'files2'
  actual = {}
  for relroot, dirnames, filenames in os.walk(root):
    for filename in filenames:
      fullpath = os.path.join(relroot, filename)
      actual[fullpath[len(root)+1:]] = open(fullpath, 'rb').read()
    if '.svn' in dirnames:
      dirnames.remove('.svn')

  if actual != expected:
    print 'Failure'
    print actual
    print expected
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())

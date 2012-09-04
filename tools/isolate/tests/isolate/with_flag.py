#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  print 'with_flag: Verify the test data files were mapped properly'
  assert len(sys.argv) == 2
  mode = sys.argv[1]
  assert mode in ('run', 'trace')
  expected = {
    os.path.join('subdir', '42.txt'):
        'the answer to life the universe and everything\n',
    'test_file1.txt': 'Foo\n',
    'test_file2.txt': 'Bar\n',
  }

  root = 'files1'
  actual = {}
  for relroot, dirnames, filenames in os.walk(root):
    for filename in filenames:
      fullpath = os.path.join(relroot, filename)
      actual[fullpath[len(root)+1:]] = open(fullpath, 'rb').read()
    if mode == 'trace' and '.svn' in dirnames:
      dirnames.remove('.svn')

  if actual != expected:
    print 'Failure'
    print actual
    print expected
    return 1

  root_dir = os.path.dirname(os.path.abspath(__file__))
  parent_dir, base = os.path.split(root_dir)
  if mode == 'trace':
    # Verify the parent directory.
    parent_dir, base2 = os.path.split(parent_dir)
    if base != 'isolate' or base2 != 'tests':
      print 'mode trace: Invalid root dir %s' % root_dir
      return 4
  else:
    # Verify that we are not inside a checkout.
    if base == 'tests':
      print 'mode run: Invalid root dir %s' % root_dir
      return 5
  return 0


if __name__ == '__main__':
  sys.exit(main())

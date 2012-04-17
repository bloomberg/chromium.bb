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
  files = sorted(os.listdir('files1'))
  tree = {
    'test_file1.txt': 'Foo\n',
    'test_file2.txt': 'Bar\n',
  }

  # Ignore .svn directory which happens to be there with --mode=trace
  # from a svn checkout. The file shouldn't be there when --mode=run is used.
  if mode == 'trace' and '.svn' in files:
    files.remove('.svn')

  if files != sorted(tree):
    print '%s != %s' % (files, sorted(tree))
    return 2
  for k, v in tree.iteritems():
    content = open(os.path.join('files1', k), 'rb').read()
    if v != content:
      print '%s: %r != %r' % (k, v, content)
      return 3

  root_dir = os.path.dirname(os.path.abspath(__file__))
  parent_dir, base = os.path.split(root_dir)
  if mode == 'trace':
    # Verify the parent directory.
    parent_dir, base2 = os.path.split(parent_dir)
    if base != 'isolate' or base2 != 'data':
      print 'mode trace: Invalid root dir %s' % root_dir
      return 4
  else:
    # Verify that we are not inside a checkout.
    if base == 'data':
      print 'mode run: Invalid root dir %s' % root_dir
      return 5
  return 0


if __name__ == '__main__':
  sys.exit(main())

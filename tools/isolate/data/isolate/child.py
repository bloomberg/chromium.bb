#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  print 'child: verify the test data files were mapped properly'
  files = sorted(os.listdir('files1'))
  tree = {
    'test_file1.txt': 'Foo\n',
    'test_file2.txt': 'Bar\n',
  }

  # For now, ignore .svn directory, which happens to be there with --mode=trace
  # from a svn checkout.  The file shouldn't be there when --mode=run is used.
  # TODO(maruel): Differentiate between the two modes and detect .svn
  # directories in --mode=run.
  if '.svn' in files:
    files.remove('.svn')

  if files != sorted(tree):
    print '%s != %s' % (files, sorted(tree))
    return 2
  for k, v in tree.iteritems():
    content = open(os.path.join('files1', k), 'rb').read()
    if v != content:
      print '%s: %r != %r' % (k, v, content)
      return 3

  if sys.argv[1] == '--ok':
    return 0
  elif sys.argv[1] == '--fail':
    return 1
  return 4


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  print 'child_touch_root: Verify the relative directories'
  root_dir = os.path.dirname(os.path.abspath(__file__))
  parent_dir, base = os.path.split(root_dir)
  parent_dir, base2 = os.path.split(parent_dir)
  if base != 'isolate' or base2 != 'tests':
    print 'Invalid root dir %s' % root_dir
    return 4

  open(os.path.join(parent_dir, 'isolate.py'), 'r').close()
  return 0


if __name__ == '__main__':
  sys.exit(main())

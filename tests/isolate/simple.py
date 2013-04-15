#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  actual = set(os.listdir('.'))
  expected = set(['simple.py'])
  if expected != actual:
    print('Unexpected files: %s' % ', '.join(sorted(actual- expected)))
    return 1
  print('Simply works.')
  return 0


if __name__ == '__main__':
  sys.exit(main())

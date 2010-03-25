#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper script that lists the header files in the passed in argument.
This is used by gyp to get a list of header files to copy."""

import os
import sys

def main(argv):
  if len(argv) != 2:
    print 'USAGE: %s <dir>'
    return 1

  for filename in os.listdir(argv[1]):
    if filename.endswith('.h'):
      print os.path.join(argv[1], filename)
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))

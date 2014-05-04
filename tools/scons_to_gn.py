#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import sys

import scons_to_gn


"""Convert from Scons to GN.

Takes a set of SCONS input files and generates a output file per input, as
well as a global alias file.
"""

def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', dest='verbose', default=False)
  options, args = parser.parse_args(argv)
  for name in args:
    tracker = scons_to_gn.ObjectTracker(name)
    tracker.Dump(sys.stdout)
  return 0


if __name__ == '__main__':
  retval = main(sys.argv[1:])
  sys.exit(retval)

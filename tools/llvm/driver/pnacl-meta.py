#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# Show the PNaCl metadata of a bitcode file

from driver_tools import *

EXTRA_ENV = {
  'INPUTS'   : '',
}
env.update(EXTRA_ENV)

META_PATTERNS = [
  ( '(.*)',      "env.append('INPUTS', pathtools.normalize($0))"),
]

def Usage():
  print "Usage: pnacl-meta [files...]"
  print "Show the PNaCl-specific metadata of a bitcode file"

def main(argv):
  ParseArgs(argv, META_PATTERNS)

  inputs = env.get('INPUTS')

  if not inputs:
    Usage()
    DriverExit(0)

  for f in inputs:
    if not IsBitcode(f):
      Log.Fatal("%s: File is not bitcode", pathtools.touser(f))
    metadata = GetBitcodeMetadata(f)
    print pathtools.touser(f) + ":"
    for k, v in metadata.iteritems():
      if isinstance(v, list):
        v = "[ " + ', '.join(v) + " ]"
      print "  %-12s: %s" % (k, v)

  return 0

if __name__ == "__main__":
  DriverMain(main)


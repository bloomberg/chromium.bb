#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# Show the PNaCl metadata of a bitcode file

import driver_tools
import pathtools
from driver_env import env
from driver_log import Log, DriverExit

EXTRA_ENV = {
  # Raw (parsable) output
  'RAW'      : '0',
  'INPUTS'   : '',
}
env.update(EXTRA_ENV)

META_PATTERNS = [
  ( '--raw',     "env.set('RAW', '1')"),
  ( '(.*)',      "env.append('INPUTS', pathtools.normalize($0))"),
]

def Usage():
  print "Usage: pnacl-meta [files...]"
  print "Show the PNaCl-specific metadata of a bitcode file"

def main(argv):
  driver_tools.ParseArgs(argv, META_PATTERNS)

  inputs = env.get('INPUTS')

  if not inputs:
    Usage()
    DriverExit(1)

  for f in inputs:
    if not driver_tools.IsBitcode(f):
      Log.Fatal("%s: File is not bitcode", pathtools.touser(f))
    metadata = driver_tools.GetBitcodeMetadata(f)
    if env.getbool('RAW'):
      DumpRaw(metadata)
    else:
      DumpPretty(f, metadata)
  return 0

def DumpPretty(f, metadata):
    print pathtools.touser(f) + ":"
    for k, v in metadata.iteritems():
      if isinstance(v, list):
        v = "[ " + ', '.join(v) + " ]"
      print "  %-12s: %s" % (k, v)

def DumpRaw(metadata):
  for k, v in metadata.iteritems():
    if not isinstance(v, list):
      print "%s: %s" % (k, v)
    else:
      for u in v:
        print "%s: %s" % (k, u)

if __name__ == "__main__":
  driver_tools.DriverMain(main)

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

from driver_tools import *

EXTRA_ENV = {
  'SINGLE_BC_LIBS'   : '0',
}
env.update(EXTRA_ENV)


def main(argv):
  env.set('ARGV', *argv)

  if env.getbool('SINGLE_BC_LIBS'): return ExperimentalRanlibHack(argv)

  return RunWithLog('"${RANLIB}" ${ARGV}', errexit = False)

def ExperimentalRanlibHack(argv):
  assert len(argv) > 0 and not argv[-1].startswith('-')
  if IsBitcode(argv[-1]):
    Log.Info('SKIPPING ranlib for bitcode file: %s', argv[-1])
    return 0
  else:
    return RunWithLog('"${RANLIB}" ${ARGV}', errexit = False)

if __name__ == "__main__":
  DriverMain(main)


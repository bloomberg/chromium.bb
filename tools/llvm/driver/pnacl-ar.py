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
  'SINGLE_BC_LIBS': '0',  # 'archive' bitcode libs as single bitcode file
}
env.update(EXTRA_ENV)

def main(argv):
  env.set('ARGV', *argv)
  if env.getbool('SINGLE_BC_LIBS'): return ExperimentalArHack(argv)
  return RunWithLog('${AR} ${ARGV}', errexit = False)

# The ar/ranlib hacks are an attempt to iron out problems with shared
# libraries e.g. duplicate symbols in different libraries, before we have
# fully functional shared libs in pnacl.
def ExperimentalArHack(argv):
  mode = argv[0]
  output = argv[1]
  inputs = argv[2:]
  # NOTE: The checks below are a little on the paranoid side
  #       in most cases exceptions can be tolerated.
  #       It is probably still worth having a look at them first
  #       so we assert on everything unusual for now.
  if mode in ['x']:
    # This is used by by newlib to repackage a bunch of given archives
    # into a new archive. It assumes all the objectfiles can be found
    # via the glob, *.o
    assert len(inputs) == 0
    shutil.copyfile(output, output.replace('..','').replace('/','_') + '.o')
    return 0
  elif mode in ['cru']:
    # NOTE: we treat cru just like rc which just so happens to work
    #       with newlib but does not work in the general case.
    #       However, due to some idiosyncrasiers in newlib we need to prune
    #       duplicates.
    inputs = list(set(inputs))
  elif mode not in ['rc']:
    Log.Fatal('Unexpected "ar" mode %s', mode)
  if not output.endswith('.a'):
    Log.Fatal('Unexpected "ar" lib %s', output)
  not_bitcode = [f for f in inputs if 'bc' != FileType(f)]
  # NOTE: end of paranoid checks
  for f in not_bitcode:
    # This is for the last remaining native lib build via scons: libcrt_platform
    if not f.endswith('tls.o') and not f.endswith('setjmp.o'):
      Log.Fatal('Unexpected "ar" arg %s', f)
  if not_bitcode:
    RunWithLog('${AR} ${ARGV}', errexit = False)
  else:
    RunWithEnv('${LLVM_LINK} ${inputs} -o ${output}',
               inputs=shell.join(inputs), output=shell.escape(output))
  return 0

if __name__ == "__main__":
  DriverMain(main)

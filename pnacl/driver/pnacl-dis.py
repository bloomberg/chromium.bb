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
from driver_env import env
from driver_log import Log, DriverOpen, DriverClose

EXTRA_ENV = {
  'INPUTS'          : '',
  'OUTPUT'          : '',
  'FLAGS'           : '',
}
env.update(EXTRA_ENV)

DISPatterns = [
  ( ('-o','(.*)'),            "env.set('OUTPUT', pathtools.normalize($0))"),
  ( '(-.*)',                  "env.append('FLAGS', $0)"),
  ( '(.*)',                   "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  ParseArgs(argv, DISPatterns)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal("No input files given")

  if len(inputs) > 1 and output != '':
    Log.Fatal("Cannot have -o with multiple inputs")

  for infile in inputs:
    env.push()
    env.set('input', infile)
    env.set('output', output)

    # When we output to stdout, set redirect_stdout and set log_stdout
    # to False to bypass the driver's line-by-line handling of stdout
    # which is extremely slow when you have a lot of output

    if IsBitcode(infile):
      if output == '':
        # LLVM by default outputs to a file if -o is missing
        # Let's instead output to stdout
        env.set('output', '-')
        env.append('FLAGS', '-f')
      RunWithLog('${LLVM_DIS} ${FLAGS} ${input} -o ${output}',
                 log_stdout = False)
    elif IsELF(infile):
      flags = env.get('FLAGS')
      if len(flags) == 0:
        env.append('FLAGS', '-d')
      if output == '':
        # objdump to stdout
        RunWithLog('${OBJDUMP} ${FLAGS} ${input}',
                   log_stdout = False, log_stderr = False)
      else:
        # objdump always outputs to stdout, and doesn't recognize -o
        # Let's add this feature to be consistent.
        fp = DriverOpen(output, 'w')
        RunWithLog('${OBJDUMP} ${FLAGS} ${input}',
                   echo_stdout = False, log_stdout = False,
                   redirect_stdout = fp)
        DriverClose(fp)
    else:
      Log.Fatal('Unknown file type')
    env.pop()

  return 0

if __name__ == "__main__":
  DriverMain(main)

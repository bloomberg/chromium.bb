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
  'INPUTS'             : '',
  'OUTPUT'             : '',
  'MODE'               : 'all',
  'OPT_FLAGS_all'      : '-disable-opt --strip',
  'OPT_FLAGS_debug'    : '-disable-opt --strip-debug',
  'STRIP_FLAGS_all'    : '-s',
  'STRIP_FLAGS_debug'  : '-S',

  'OPT_FLAGS'          : '${OPT_FLAGS_%MODE%}',
  'STRIP_FLAGS'        : '${STRIP_FLAGS_%MODE%}',

  'RUN_OPT'            : '${LLVM_OPT} ${OPT_FLAGS} ${input} -o ${output}',
  'RUN_STRIP'          : '${STRIP} ${STRIP_FLAGS} ${input} -o ${output}',
}
env.update(EXTRA_ENV)

StripPatterns = [
    ( ('-o','(.*)'),            "env.set('OUTPUT', $0)"),
    ( '--strip-all',            "env.set('MODE', 'all')"),
    ( '-s',                     "env.set('MODE', 'all')"),

    ( '--strip-debug',          "env.set('MODE', 'debug')"),
    ( '-S',                     "env.set('MODE', 'debug')"),
    ( '(-.*)',                  UnrecognizedOption),

    ( '(.*)',                   "env.append('INPUTS', $0)"),
]

def main(argv):
  ParseArgs(argv, StripPatterns)
  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) > 1 and output != '':
    Log.Fatal('Cannot have -o with multiple inputs')

  for f in inputs:
    if output != '':
      f_output = output
    else:
      f_output = f
    if IsBitcode(f):
      RunWithEnv('${RUN_OPT}', input = f, output = f_output)
    elif IsELF(f):
      RunWithEnv('${RUN_STRIP}', input = f, output = f_output)
    else:
      Log.Fatal('%s: File is neither ELF nor bitcode', f)
  return 0

if __name__ == "__main__":
  DriverMain(main)

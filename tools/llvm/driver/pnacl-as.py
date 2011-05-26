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
  'INPUTS'      : '',
  'OUTPUT'      : '',

  # Options
  'DIAGNOSTIC'  : '0',
  'USE_MC_ASM'  : '1',    # Use llvm-mc instead of nacl-as

  'AS_FLAGS'      : '${AS_FLAGS_%ARCH%}',
  'AS_FLAGS_ARM'  : '-mfpu=vfp -march=armv7-a',
  'AS_FLAGS_X8632': '--32 --nacl-align 5 -n -march=pentium4 -mtune=i386',
  'AS_FLAGS_X8664': '--64 --nacl-align 5 -n -mtune=core2',

  'MC_FLAGS'       : '${MC_FLAGS_%ARCH%}',
  'MC_FLAGS_ARM'   : '-assemble -filetype=obj -arch=arm -triple=armv7a-nacl',
  'MC_FLAGS_X8632' : '-assemble -filetype=obj -arch=i686 -triple=i686-nacl',
  'MC_FLAGS_X8664' : '-assemble -filetype=obj -arch=x86_64 -triple=x86_64-nacl',

  'AS'            : '${SANDBOXED ? ${AS_SB} : ${AS_%ARCH%}}',

  'RUN_LLVM_AS' : '${LLVM_AS} ${input} -o ${output}',
  'RUN_NACL_AS' : '${AS} ${AS_FLAGS} ${input} -o ${output}',
  'RUN_LLVM_MC' : '${LLVM_MC} ${MC_FLAGS} ${input} -o ${output}',


}
env.update(EXTRA_ENV)

ASPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '(-v|--version)',  "env.set('DIAGNOSTIC', '1')"),

  # Ignore these assembler flags
  ( '(-Qy)',                  ""),
  ( ('(--traditional-format)', '.*'), ""),
  ( '(-gstabs)',              ""),
  ( '(--gstabs)',             ""),
  ( '(-gdwarf2)',             ""),
  ( '(--gdwarf2)',            ""),
  ( '(--fatal-warnings)',     ""),
  ( '(-meabi=.*)',            ""),
  ( '(-mfpu=.*)',             ""),
  ( '(-march=.*)',            ""),

  ( '(-.*)',  UnrecognizedOption),

  # Unmatched parameters should be treated as
  # assembly inputs by the "as" incarnation.
  ( '(.*)',  "env.append('INPUTS', $0)"),
]

def main(argv):
  ParseArgs(argv, ASPatterns)
  arch = GetArch()

  if env.getbool('DIAGNOSTIC'):
    GetArch(required = True)
    env.set('ARGV', *argv)
    RunWithLog('${AS} ${ARGV}')
    return 0

  # Disable MC ASM and sandboxed AS for ARM until we have it working
  if arch == 'ARM':
    env.set('USE_MC_ASM', '0')
    env.set('SANDBOXED', '0')

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')

  if arch:
    output_type = 'o'
  else:
    output_type = 'bc'

  if output == '':
    output = 'a.out'

  env.push()
  env.set('input', inputs[0])
  env.set('output', output)

  if output_type == 'bc':
    # .ll to .bc
    RunWithLog("${RUN_LLVM_AS}")
  else:
    # .s to .o
    if env.getbool('USE_MC_ASM'):
      RunWithLog("${RUN_LLVM_MC}")
    else:
      RunWithLog("${RUN_NACL_AS}")
  env.pop()
  return 0



if __name__ == "__main__":
  DriverMain(main)

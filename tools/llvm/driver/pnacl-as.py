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

  'AS_FLAGS_ARM'  : '-mfpu=vfp -march=armv7-a',
  # once we can use llvm's ARM assembler we should use these flags
  #'AS_FLAGS_ARM'   : '-assemble -filetype=obj -arch=arm -triple=armv7a-nacl',
  'AS_FLAGS_X8632' : '-assemble -filetype=obj -arch=i686 -triple=i686-nacl',
  'AS_FLAGS_X8664' : '-assemble -filetype=obj -arch=x86_64 -triple=x86_64-nacl',

  'RUN_BITCODE_AS' : '${LLVM_AS} ${input} -o ${output}',
  'RUN_NATIVE_AS' : '${AS_%ARCH%} ${AS_FLAGS_%ARCH%} ${input} -o ${output}',
}
env.update(EXTRA_ENV)

ASPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

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
  ( '(.*)',  "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  ParseArgs(argv, ASPatterns)
  arch = GetArch()

  if env.getbool('DIAGNOSTIC'):
    GetArch(required=True)
    env.set('ARGV', *argv)
    # NOTE: we could probably just print a canned string out instead.
    RunWithLog('${AS_%ARCH%} ${ARGV}')
    return 0

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')

  if arch:
    output_type = 'o'
  else:
    output_type = 'po'

  if output == '':
    output = 'a.out'

  env.push()
  env.set('input', inputs[0])
  env.set('output', output)

  if output_type == 'po':
    # .ll to .po
    RunWithLog("${RUN_BITCODE_AS}")
  else:
    # .s to .o
    RunWithLog("${RUN_NATIVE_AS}")
  env.pop()
  return 0


if __name__ == "__main__":
  DriverMain(main)

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

import driver_tools

env = driver_tools.env

EXTRA_ENV = {
  'INPUTS'      : '',
  'OUTPUT'      : '',

  # Options
  'DIAGNOSTIC'  : '0',
  'ASPP_FLAGS': '-DNACL_LINUX=1',

  'AS_FLAGS_ARM'  : '-mfpu=vfp -march=armv7-a',
  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=1968
  # once we can use llvm's ARM assembler we should use these flags
  #'AS_FLAGS_ARM'   : '-assemble -filetype=obj -arch=arm -triple=armv7a-nacl',
  'AS_FLAGS_X8632' : '-assemble -filetype=obj -arch=x86 -triple=i686-nacl',
  'AS_FLAGS_X8664' : '-assemble -filetype=obj -arch=x86-64 -triple=x86_64-nacl',

  'RUN_BITCODE_AS' : '${LLVM_AS} ${input} -o ${output}',
  'RUN_NATIVE_AS' : '${AS_%ARCH%} ${AS_FLAGS_%ARCH%} ${input} -o ${output}',

  # NOTE: should we run the vanilla preprocessor instead?
  'RUN_PP' : '${CLANG} -E ${ASPP_FLAGS} ${input} -o ${output}'
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

  ( '-c',                     ""),

  ( ('-I', '(.+)'),    "env.append('ASPP_FLAGS', '-I'+pathtools.normalize($0))"),
  ( '-I(.+)',          "env.append('ASPP_FLAGS', '-I'+pathtools.normalize($0))"),

  ( ('(-D)','(.*)'),          "env.append('ASPP_FLAGS', $0, $1)"),
  ( '(-D.+)',                 "env.append('ASPP_FLAGS', $0)"),

  ( '(-.*)',  driver_tools.UnrecognizedOption),

  # Unmatched parameters should be treated as
  # assembly inputs by the "as" incarnation.
  ( '(.*)',  "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  driver_tools.ParseArgs(argv, ASPatterns)
  arch = driver_tools.GetArch()

  if env.getbool('DIAGNOSTIC'):
    driver_tools.GetArch(required=True)
    env.set('ARGV', *argv)
    # NOTE: we could probably just print a canned string out instead.
    driver_tools.RunWithLog('${AS_%ARCH%} ${ARGV}')
    return 0

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')
  the_input = inputs[0]


  if arch:
    output_type = 'o'
  else:
    output_type = 'po'

  if output == '':
    output = 'a.out'

  if the_input.endswith('.S'):
    tmp_output = output + ".s"
    driver_tools.TempFiles.add(tmp_output)
    env.push()
    env.set('input', the_input)
    env.set('output', tmp_output)
    driver_tools.RunWithLog("${RUN_PP}")
    env.pop()
    the_input = tmp_output

  env.push()
  env.set('input', the_input)
  env.set('output', output)

  if output_type == 'po':
    # .ll to .po
    driver_tools.RunWithLog("${RUN_BITCODE_AS}")
  else:
    # .s to .o
    driver_tools.RunWithLog("${RUN_NATIVE_AS}")
  env.pop()
  return 0


if __name__ == "__main__":
  driver_tools.DriverMain(main)

#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import driver_tools
import pathtools
from driver_env import env
from driver_log import Log

EXTRA_ENV = {
  'INPUTS'             : '',
  'OUTPUT'             : '',
  'MODE'               : 'all',
  'DO_WRAP'            : '1',

  'OPT_FLAGS_all'      : '-disable-opt --strip',
  'OPT_FLAGS_debug'    : '-disable-opt --strip-debug',
  'STRIP_FLAGS_all'    : '-s',
  'STRIP_FLAGS_debug'  : '-S',

  'OPT_FLAGS'          : '${OPT_FLAGS_%MODE%}',
  'STRIP_FLAGS'        : '${STRIP_FLAGS_%MODE%}',

  'RUN_OPT'            : '${LLVM_OPT} ${OPT_FLAGS} ${input} -o ${output}',
  'RUN_STRIP'          : '${STRIP} ${STRIP_FLAGS} ${input} -o ${output}',
}

StripPatterns = [
    ( ('-o','(.*)'),     "env.set('OUTPUT', pathtools.normalize($0))"),
    ( ('-o','(.*)'),     "env.set('OUTPUT', pathtools.normalize($0))"),

    ( '--do-not-wrap',   "env.set('DO_WRAP', '0')"),

    ( '--strip-all',     "env.set('MODE', 'all')"),
    ( '-s',              "env.set('MODE', 'all')"),

    ( '--strip-debug',   "env.set('MODE', 'debug')"),
    ( '-S',              "env.set('MODE', 'debug')"),
    ( '-g',              "env.set('MODE', 'debug')"),
    ( '-d',              "env.set('MODE', 'debug')"),

    ( '(-p)',            "env.append('STRIP_FLAGS', $0)"),
    ( '(--info)',        "env.append('STRIP_FLAGS', $0)"),

    ( '(-.*)',           driver_tools.UnrecognizedOption),

    ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, StripPatterns)
  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) > 1 and output != '':
    Log.Fatal('Cannot have -o with multiple inputs')

  if '--info' in env.get('STRIP_FLAGS'):
    code, _, _ = driver_tools.Run('${STRIP} ${STRIP_FLAGS}')
    return code

  for f in inputs:
    if output != '':
      f_output = output
    else:
      f_output = f
    if driver_tools.IsBitcode(f):
      driver_tools.RunWithEnv('${RUN_OPT}', input=f, output=f_output)
      if env.getbool('DO_WRAP'):
        driver_tools.WrapBitcode(f_output)
    elif driver_tools.IsELF(f):
      driver_tools.RunWithEnv('${RUN_STRIP}', input=f, output=f_output)
    else:
      Log.Fatal('%s: File is neither ELF nor bitcode', pathtools.touser(f))
  return 0


def get_help(unused_argv):
  script = env.getone('SCRIPT_NAME')
  return """Usage: %s <option(s)> in-file(s)
  Removes symbols and sections from bitcode or native code.

  The options are:
  -p --preserve-dates              Copy modified/access timestamps to the output
                                   (only native code for now)
  -s --strip-all                   Remove all symbol and relocation information
  -g -S -d --strip-debug           Remove all debugging symbols & sections
  -h --help                        Display this output
     --info                        List object formats & architectures supported
  -o <file>                        Place stripped output into <file>

%s: supported targets: bitcode, native code (see --info).
""" % (script, script)

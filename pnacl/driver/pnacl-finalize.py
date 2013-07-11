#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import shutil

from driver_env import env
from driver_log import Log
import driver_tools
import filetype
import pathtools


EXTRA_ENV = {
  'INPUTS'             : '',
  'OUTPUT'             : '',
  'DISABLE_FINALIZE'   : '0',
}

PrepPatterns = [
    ( ('-o','(.*)'),     "env.set('OUTPUT', pathtools.normalize($0))"),
    ( '--no-finalize',   "env.set('DISABLE_FINALIZE', '1')"),
    ( '(-.*)',           driver_tools.UnrecognizedOption),
    ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, PrepPatterns)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Can only have one input')
  f_input = inputs[0]

  # Allow in-place file changes if output isn't specified..
  if output != '':
    f_output = output
  else:
    f_output = f_input

  if env.getbool('DISABLE_FINALIZE') or filetype.IsPNaClBitcode(f_input):
    # Just copy the input file to the output file.
    if f_input != f_output:
      shutil.copyfile(f_input, f_output)
    return 0

  opt_flags = ['-disable-opt', '-strip', '-strip-metadata',
               '--bitcode-format=pnacl', f_input, '-o', f_output]
  # Transform the file, and convert it to a PNaCl bitcode file.
  driver_tools.RunDriver('opt', opt_flags)
  return 0


def get_help(unused_argv):
  script = env.getone('SCRIPT_NAME')
  return """Usage: %s <options> in-file
  This tool prepares a PNaCl bitcode application for ABI stability.

  The options are:
  -h --help                 Display this output
  -o <file>                 Place the output into <file>. Otherwise, the
                            input file is modified in-place.
  --no-finalize             Don't run preparation steps (just copy in -> out).
""" % script

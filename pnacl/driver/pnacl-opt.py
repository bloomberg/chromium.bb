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

from driver_env import env
from driver_log import Log
import driver_tools
import subprocess

EXTRA_ENV = {
  'DO_WRAP': '1',
  'ARGS'   : '',
  'OUTPUT' : '',
  'HAVE_OUTPUT' : '0',
}

PATTERNS  = [
  ( '--do-not-wrap',   "env.set('DO_WRAP', '0')"),
  (('-o','(.*)'),      "env.set('OUTPUT', pathtools.normalize($0))\n" +
                       "env.set('HAVE_OUTPUT', '1')"),
  ( '(.*)',            "env.append('ARGS', $0)"),
]

def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, PATTERNS)

  # Binary output may go to stdout (when -o was not specified)
  driver_tools.Run('"${LLVM_OPT}" ${ARGS} ${HAVE_OUTPUT ? -o ${OUTPUT}}')

  if env.getbool('DO_WRAP'):
    if not env.getbool('HAVE_OUTPUT'):
      Log.Error("unable to wrap pexe on stdout, use: --do-no-wrap flag")
    else:
      driver_tools.WrapBitcode(env.getone('OUTPUT'))
  # only reached in case of no errors
  return 0

def get_help(unused_argv):
  # Set errexit=False -- Do not exit early to allow testing.
  # For some reason most the llvm tools return non-zero with --help,
  # while all of the gnu tools return 0 with --help.
  # On windows, the exit code is also inconsistent =(
  code, stdout, stderr = driver_tools.Run('${LLVM_OPT} -help',
                                          redirect_stdout=subprocess.PIPE,
                                          redirect_stderr=subprocess.STDOUT,
                                          errexit=False)
  return stdout

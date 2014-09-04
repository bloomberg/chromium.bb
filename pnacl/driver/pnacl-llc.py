#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a thin wrapper for native LLC. This is not meant to be used by
# the user, only by the build system.

import subprocess

from driver_env import env
import driver_tools

EXTRA_ENV = {
  'ARGS'   : '',
  'INPUT'  : '',
  'OUTPUT' : '',
  # Binary output may go to stdout (when -o was not specified)
  'HAVE_OUTPUT' : '0',
}

PATTERNS  = [
  (('-o','(.*)'),      "env.set('OUTPUT', pathtools.normalize($0))\n" +
                       "env.set('HAVE_OUTPUT', '1')"),
  ( '(-.*)',           "env.append('ARGS', $0)"),
  ( '(.*)',            "env.set('INPUT', $0)"),
]

def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, PATTERNS)

  driver_tools.CheckPathLength(env.getone('INPUT'))
  driver_tools.CheckPathLength(env.getone('OUTPUT'))

  driver_tools.Run(
      '"${LLVM_PNACL_LLC}" ${ARGS} ' +
      '${HAVE_OUTPUT ? -o ${OUTPUT}} ' +
      '${INPUT}')

  # only reached in case of no errors
  return 0

def get_help(unused_argv):
  # Set errexit=False -- Do not exit early to allow testing.
  # For some reason most the llvm tools return non-zero with --help,
  # while all of the gnu tools return 0 with --help.
  # On windows, the exit code is also inconsistent =(
  code, stdout, stderr = driver_tools.Run('${LLVM_PNACL_LLC} -help',
                                          redirect_stdout=subprocess.PIPE,
                                          redirect_stderr=subprocess.STDOUT,
                                          errexit=False)

  help = """
LLVM static compiler

To translate PNaCl bitcode, use pnacl-translate instead.

""" + stdout

  return help

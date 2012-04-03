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
from driver_env import env

OptOutputPatterns = [
    # This script only cares about the output file (for bitcode wrapping).
    # The rest of the args are ignored and passed through to llvm opt
    (('-o','(.*)'), "env.set('OUTPUT', pathtools.normalize($0))"),
    ('.*', ""),
]

def main(argv):
  retcode = RunOpt(argv)
  driver_tools.ParseArgs(argv, OptOutputPatterns)
  if retcode == 0 and not env.getbool('RECURSE') and env.has('OUTPUT'):
    # Do not wrap if we are called by some other driver component, or
    # if the output is going to stdout
    retcode = driver_tools.WrapBitcode(env.getone('OUTPUT'))
  return retcode

def get_help(unused_argv):
  RunOpt(['--help'])
  return ""

def RunOpt(args):
  # Binary output may go to stdout
  env.set('ARGS', *args)
  retcode, _, _ = driver_tools.RunWithLog('"${LLVM_OPT}" ${ARGS}',
                                          errexit=False,
                                          log_stdout=False)
  return retcode

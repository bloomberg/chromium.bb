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
from driver_log import Log

EXTRA_ENV = {
  'DO_WRAP': '1',
}

# Note we are too lazy to enumerate all the opt flags which would
# just be passed through anyway hence we use two stage args parsing.
# TODO(robertm): clean this up
OptNoWrapPattern = [
  ( '--do-not-wrap',   "env.set('DO_WRAP', '0')"),
]

OptOutputPatterns = [
  (('-o','(.*)'), "env.set('OUTPUT', pathtools.normalize($0))"),
]

def main(argv):
  env.update(EXTRA_ENV)
  _, argv = driver_tools.ParseArgs(argv, OptNoWrapPattern, must_match=False)
  RunOpt(argv)
  driver_tools.ParseArgs(argv, OptOutputPatterns, must_match=False)
  if env.getbool('DO_WRAP'):
    output = env.getone('OUTPUT')
    if not output:
       Log.Error("unable to wrap pexe on stdout, use: --do-no-wrap flag")
    else:
      driver_tools.WrapBitcode(output)
  # only reached in case of no errors
  return 0

def get_help(unused_argv):
  RunOpt(['--help'])
  return ""

def RunOpt(args):
  # Binary output may go to stdout
  env.set('ARGS', *args)
  driver_tools.Run('"${LLVM_OPT}" ${ARGS}')


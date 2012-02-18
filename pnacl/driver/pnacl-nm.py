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

from driver_tools import RunWithLog
from driver_env import env

def main(argv):
  env.set('ARGS', *argv)
  return RunWithLog('${NM} --plugin=LLVMgold ${ARGS}', errexit=False)

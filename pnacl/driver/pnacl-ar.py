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

import shutil
import driver_tools
from driver_env import env
from driver_log import Log

def main(argv):
  env.set('ARGS', *argv)
  return driver_tools.RunWithLog('${AR} ${ARGS}', errexit = False)

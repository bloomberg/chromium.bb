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
  # TODO(robertm): move this into the TC once this matures
  'PEXE_ALLOWED_UNDEFS': '${BASE_NACL}/tools/llvm/non_bitcode_symbols.txt',

  'RUN_PEXECHECK': '${LLVM_LD} --nacl-abi-check ' +
                   '--nacl-legal-undefs ${PEXE_ALLOWED_UNDEFS} ${inputs}',
}
env.update(EXTRA_ENV)

def main(argv):
  """Check whether a pexe satifies our ABI rules"""
  return RunWithEnv('${RUN_PEXECHECK}', inputs=argv[0], errexit = False)

if __name__ == "__main__":
  DriverMain(main)

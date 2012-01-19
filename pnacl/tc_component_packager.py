#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
This script extracts sub-components from a fully populated
toolchain/pnacl_linux_XXX/ directory to create custom toolchains.
It is intentioanlly kept simple.

For usage information run:
     pnacl/tc_component_packager.py -h
'''

import glob
import optparse
import os
import sys

######################################################################
# BEGINING OF TABLES
######################################################################
COMPONENTS = {}

COMPONENTS['bitcode_newlib_frontend'] = [
    # basics
    'newlib/lib/',      # this probably includes too much
    'newlib/sysroot/',
    'newlib/usr/include/',
    'pkg/llvm/lib/clang/3.1/include/',

    # bitcode libs
    'newlib/usr/lib/libc.a',
    'newlib/usr/lib/libm.a',
    'newlib/usr/lib/libstdc++.a',

    # more libs and headers (from nacl-ports)
    'newlib/sdk/',

    # magic file
    'newlib/bin/newlib.cfg',

    # tool wrappers
    'newlib/bin/driver_*',
    'newlib/bin/*tools.py',
    'newlib/bin/pnacl-driver',
    'newlib/bin/pnacl-clang*',
    'newlib/bin/pnacl-nm',
    'newlib/bin/pnacl-opt',
    'newlib/bin/pnacl-ar',
    'newlib/bin/pnacl-ld',
    'newlib/bin/pnacl-ranlib',
    'newlib/bin/pnacl-strip',
    'newlib/bin/pnacl-nop',       # probably not really need but tiny
    'newlib/bin/pnacl-illegal',   # probably not really need but tiny

    # actual tools
    'pkg/llvm/bin/clang*',
    'pkg/llvm/bin/llvm-ar',
    'pkg/llvm/bin/llvm-nm',
    'pkg/llvm/bin/llvm-ranlib',
    'pkg/llvm/bin/llvm-ld',
    'pkg/llvm/bin/llvm-link',
    'pkg/llvm/bin/opt',
    'pkg/binutils/bin/arm-pc-nacl-ar',
    'pkg/binutils/bin/arm-pc-nacl-ranlib',
    'pkg/llvm/lib/libLLVMgold.so',    # symlink
    'pkg/llvm/lib/LLVMgold.so',
    'pkg/llvm/lib/libLTO.so',
    'pkg/binutils/lib/bfd-plugins', # used by arm-pc-nacl-ranlib (important!)
    'pkg/binutils/bin/arm-pc-nacl-ld.gold',
]

COMPONENTS['translator_newlib_backend'] = [
    'lib-arm/lib*.a',
    'lib-arm/crt*.o',
    'lib-x86-32/lib*.a',
    'lib-x86-32/crt*.o',
    'lib-x86-64/lib*.a',
    'lib-x86-64/crt*.o',
    # we only need llc and ld
    # NOTE: these are not sandboxed and  multi arch!
    'pkg/binutils/bin/arm-pc-nacl-ld.bfd',
    'pkg/binutils/bin/arm-pc-nacl-strip',
    # NOTE: while this is linked shared the llvm libs are linked in static
    'pkg/llvm/bin/llc',
    'newlib/bin/pnacl-translate',
]

# validator, etc.
COMPONENTS['translator_tools'] = [
    # TBD
]

# misc magic for exotic use cases in scons
COMPONENTS['misc_magic_newlib'] = [
    'newlib/bin/pnacl-nativeld',
    'newlib/bin/pnacl-as',
    'pkg/llvm/bin/llvm-mc',
]

def Composite(*lst):
  combined = [COMPONENTS[x] for x in lst]
  return sum(combined, [])

# This is what should go to regular developers as a nacl-gcc replacement
COMPONENTS['newlib_sdk'] = Composite('bitcode_newlib_frontend',
                                     'translator_newlib_backend',
                                     'translator_tools')

# also contains some tools for handling exotic use cases
# this is suitable of handling all scons tests
COMPONENTS['newlib_sdk_plus'] = Composite('newlib_sdk', 'misc_magic_newlib')


######################################################################
# END OF TABLES
######################################################################

def RunCommand(cmd):
  print cmd
  err = os.system(cmd)
  if err:
    sys.exit(-1)

def MaybeMkdir(path):
  if os.path.isdir(path):
    return
  RunCommand('mkdir -p %s' % path)

def CopyMatching(src, dst, patterns):
  '''copies files from src/ to dst/ if the match
  the list of 'glob' patters.
  '''
  MaybeMkdir(dst)
  for p in patterns:
    matches = glob.glob(src + '/' + p)
    for m in matches:
      if m.endswith('/'):
        m = m[:-1]
      dirname = os.path.dirname(m).replace(src, dst, 1)
      MaybeMkdir(dirname)
      RunCommand('cp -a %(m)s %(dirname)s' % locals())


def Main():
   usage = 'usage: %prog [options] <src-path> <dst-path> <component>'
   parser = optparse.OptionParser(usage)
   options, args = parser.parse_args()

   assert len(args) == 3
   src, dst, patterns = args
   assert patterns in  COMPONENTS
   assert os.path.exists(src)

   CopyMatching(src, dst, COMPONENTS[patterns])

if __name__ == '__main__':
   sys.exit(Main())

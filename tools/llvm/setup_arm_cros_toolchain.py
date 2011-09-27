#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

#########################################################################
# EXPERIMENTAL
# this code is NOT active in the main NaCl yet!
# this is only for testing within an artificial CrOS hook for ARM/thumb2
# BUG=http://code.google.com/p/chromium/issues/detail?id=61695
# BUG=http://code.google.com/p/chromium/issues/detail?id=38909
# BUG=http://code.google.com/p/nativeclient/issues/detail?id=135
#########################################################################

import os
import sys

if os.path.basename(os.getcwd()) != 'native_client':
  print "Error: Run this script from native_client/"
  sys.exit(1)

# Alias for convenience
J = os.path.join

NACL_ROOT = os.getcwd()
BASE_DIR = J(NACL_ROOT, 'toolchain', 'linux_arm-trusted')
ARM_CROSS_TARGET = 'armv7a-cros-linux-gnueabi'

# do we need this stuff at all?
# CODE_SOURCERY_PREFIX = J(BASE_DIR, 'arm-2009q3', 'bin', ARM_CROSS_TARGET)
CODE_SOURCERY_PREFIX = ""
CODE_SOURCERY_JAIL = J(BASE_DIR, 'arm-2009q3', ARM_CROSS_TARGET, 'libc')
LD_SCRIPT_TRUSTED = J(BASE_DIR, 'ld_script_arm_trusted')

# so do we need to explicitly use the shell scripts in this directory?
# "/usr/x86_64-pc-linux-gnu/armv7a-cros-linux-gnueabi/gcc-bin/4.4.3/"
DEFAULT_CROS_DIR = ""
CROS_TOOLCHAIN = "%s%s" % (DEFAULT_CROS_DIR, ARM_CROSS_TARGET)

##
BASE_CC = ("%s-%%s "
           "-Werror -O2 "
           "-fdiagnostics-show-option "
           "%%s "
           ) % (CROS_TOOLCHAIN)

# Shell exports
ARM_CC = BASE_CC % ('gcc', '-std=gnu99 -pedantic')
ARM_CXX = BASE_CC % ('g++', '')
ARM_LD = '%s-ld' % CROS_TOOLCHAIN
ARM_LINKFLAGS = '' # '-static'
ARM_LIB_DIR = J(CODE_SOURCERY_JAIL, 'usr', 'lib')
ARM_EMU = J(BASE_DIR, 'run_under_qemu_arm')

# Don't actually emit BASE_CC
del BASE_CC

shell_exports = [
  'NACL_ROOT',
  'ARM_CC',
  'ARM_CXX',
  'ARM_LD',
  'ARM_LINKFLAGS',
  'ARM_LIB_DIR',
  'ARM_EMU'
]

# Store the values into a dict
arm_env = {}
for k,v in dict(globals()).iteritems():
  if k.startswith('_') or not isinstance(v, str):
    continue
  arm_env[k] = v

if __name__ == '__main__':
  # Print the values out, for use in a shell script
  for k, v in arm_env.iteritems():
    if k in shell_exports:
      prefix = 'export '
    else:
      prefix = ''
    print '%s%s="%s";' % (prefix, k, v)

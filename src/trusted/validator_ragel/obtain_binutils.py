#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Obtain objdump and gas binaries.

Usage:
    obtain_binutils.py <objdump> <gas>

Check out appropriate version of binutils, compile it and
copy objdump and gas binaries to specified places.
"""

import os
import shutil
import sys

CHECKOUT_DIR = '/dev/shm/binutils'

BINUTILS_REPO = 'http://git.chromium.org/native_client/nacl-binutils.git'
BINUTILS_REVISION = '8c8125b61c560e6ae47d4f3e48786121f944e364'

# We need specific revision of binutils, and it have to include the
# following patches:
#   Add prefetch_modified - non-canonical encoding of prefetch
#
#   Properly handle data16+rex.w instructions, such as
#   66 48 68 01 02 03 04 data32 pushq $0x4030201
#
# We are not using head revision because decoder test depends
# on precise format of objdump output.


def Command(cmd):
  print
  print 'Running:', cmd
  print
  result = os.system(cmd)
  if result != 0:
    print 'Command returned', result
    sys.exit(1)


def main():
  if len(sys.argv) != 3:
    print __doc__
    sys.exit(1)

  # These are required to make binutils,
  # and when they are missing binutils's make
  # error messages are cryptic, so we better fail early.
  Command('flex --version')
  Command('bison --version')
  Command('makeinfo --version')

  if os.path.exists(CHECKOUT_DIR):
    shutil.rmtree(CHECKOUT_DIR)

  Command('git clone %s %s' % (BINUTILS_REPO, CHECKOUT_DIR))

  try:
    old_dir = os.getcwd()
    os.chdir(CHECKOUT_DIR)
    Command('git checkout %s' % BINUTILS_REVISION)

    Command('./configure')
    Command('make')

    os.chdir(old_dir)
    objdump, gas = sys.argv[1:]
    shutil.copy(os.path.join(CHECKOUT_DIR, 'binutils', 'objdump'), objdump)
    shutil.copy(os.path.join(CHECKOUT_DIR, 'gas', 'as-new'), gas)
  finally:
    shutil.rmtree(CHECKOUT_DIR)

  print 'ok'


if __name__ == '__main__':
  main()

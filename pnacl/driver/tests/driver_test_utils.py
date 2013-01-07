#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from os.path import dirname
import platform
import sys

def CanRunHost():
  # Some of the test+tools require running the host binaries, but that
  # does not work on some bots (e.g., the ARM bots).
  if platform.machine().startswith('arm'):
    return False
  # We also cannot run some of the Windows binaries directly, since
  # they depend on cygwin DLLs and the cygwin DLLs are only in the
  # path for the installed drivers (newlib/bin) and not for the binaries.
  if sys.platform == 'win32':
    return False
  return True

def _SetupLinuxHostDir(env, nacl_dir):
  # Use the 32-bit path by default, but fall back to 64-bit if the 32-bit does
  # not exist.
  dir_template = os.path.join(nacl_dir, 'toolchain', 'pnacl_linux_x86',
                              'host_%s')
  dir_32 = dir_template % 'x86_32'
  dir_64 = dir_template % 'x86_64'
  env.set('BASE_HOST', dir_32 if os.path.exists(dir_32) else dir_64)

def SetupHostDir(env):
  # Some of the tools require 'BASE_HOST' to be set, because they end up
  # running one of the host binaries.
  test_dir = os.path.abspath(dirname(__file__))
  nacl_dir = dirname(dirname(dirname(test_dir)))
  if sys.platform == 'darwin':
    os_shortname = 'mac'
    host_arch = 'x86_64'
  elif sys.platform.startswith('linux'):
    _SetupLinuxHostDir(env, nacl_dir)
    return
  elif sys.platform in ('cygwin', 'win32'):
    os_shortname = 'win'
    host_arch= 'x86_32'
  host_dir = os.path.join(nacl_dir, 'toolchain',
                          'pnacl_%s_x86' % os_shortname,
                          'host_%s' % host_arch)
  env.set('BASE_HOST', host_dir)

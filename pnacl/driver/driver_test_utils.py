#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
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

def SetupHostDir(env):
  # Some of the tools require 'BASE_HOST' to be set, because they end up
  # running one of the host binaries.
  test_dir = os.path.abspath(os.path.dirname(__file__))
  nacl_dir = os.path.dirname(os.path.dirname(test_dir))
  if sys.platform == 'darwin':
    os_shortname = 'mac'
    host_arch = 'x86_64'
  elif sys.platform.startswith('linux'):
    os_shortname = 'linux'
    machine = platform.machine()
    if machine == 'x86_64':
      host_arch = 'x86_64'
    elif machine in ('i386', 'i686'):
      host_arch = 'x86_32'
    else:
      raise Exception('Unknown architecture: %s' % machine)
  elif sys.platform in ('cygwin', 'win32'):
    os_shortname = 'win'
    host_arch= 'x86_32'
  host_dir = os.path.join(nacl_dir, 'toolchain',
                          'pnacl_%s_x86' % os_shortname,
                          'host_%s' % host_arch)
  env.set('BASE_HOST', host_dir)

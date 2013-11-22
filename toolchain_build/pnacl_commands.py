#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runnables for toolchain_build_pnacl.py
"""

# Done first to set up python module path
import toolchain_env
import os
import shutil
import stat

import file_tools
import platform_tools

# User-facing tools
DRIVER_TOOLS = ['pnacl-' + tool + '.py' for tool in
                    ('abicheck', 'ar', 'as', 'clang', 'clang++', 'dis',
                     'driver', 'finalize', 'ld', 'nativeld', 'nm', 'opt',
                     'ranlib', 'readelf', 'strip', 'translate')]
# Utilities used by the driver
DRIVER_UTILS = [name + '.py' for name in
                    ('artools', 'driver_env', 'driver_log', 'driver_tools',
                     'elftools', 'filetype', 'ldtools', 'loader', 'pathtools',
                     'shelltools')]

def InstallDriverScripts(subst, srcdir, dstdir):
  srcdir = subst.SubstituteAbsPaths(srcdir)
  dstdir = subst.SubstituteAbsPaths(dstdir)
  file_tools.MakeDirectoryIfAbsent(os.path.join(dstdir, 'pydir'))
  for name in DRIVER_TOOLS + DRIVER_UTILS:
    shutil.copy(os.path.join(srcdir, name), os.path.join(dstdir, 'pydir'))
  # Install redirector sh/bat scripts
  redirector = 'redirect.bat' if platform_tools.IsWindows() else 'redirect.sh'
  for name in DRIVER_TOOLS:
    # Chop the .py off the name
    nopy_name = os.path.join(dstdir, name[:-3])
    shutil.copy(os.path.join(srcdir, redirector), nopy_name)
    os.chmod(nopy_name,
             stat.S_IRUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IXGRP)
  # Install the driver.conf file
  with open(os.path.join(dstdir, 'driver.conf'), 'w') as f:
    print >> f, 'HAS_FRONTEND=1'
    print >> f, 'HOST_ARCH=x86_32'
  # On windows, copy the necessary cygwin DLLs
  if platform_tools.IsWindows():
    for lib in ('gcc_s-1', 'iconv-2', 'win1', 'intl-8', 'stdc++-6', 'z'):
      shutil.copy('/bin/cyg' + lib + '.dll', dstdir)

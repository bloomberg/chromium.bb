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
import repo_tools
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

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

def InstallDriverScripts(subst, srcdir, dstdir, host_windows=False,
                         host_64bit=False, extra_config=[]):
  srcdir = subst.SubstituteAbsPaths(srcdir)
  dstdir = subst.SubstituteAbsPaths(dstdir)
  file_tools.MakeDirectoryIfAbsent(os.path.join(dstdir, 'pydir'))
  for name in DRIVER_TOOLS + DRIVER_UTILS:
    shutil.copy(os.path.join(srcdir, name), os.path.join(dstdir, 'pydir'))
  # Install redirector sh/bat scripts
  for name in DRIVER_TOOLS:
    # Chop the .py off the name
    nopy_name = os.path.join(dstdir, os.path.splitext(name)[0])
    shutil.copy(os.path.join(srcdir, 'redirect.sh'), nopy_name)
    os.chmod(nopy_name,
             stat.S_IRUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IXGRP)
    if host_windows:
      # Windows gets both sh and bat extensions so it works w/cygwin and without
      batch_script = nopy_name + '.bat'
      shutil.copy(os.path.join(srcdir, 'redirect.bat'), batch_script)
      os.chmod(batch_script,
               stat.S_IRUSR | stat.S_IXUSR | stat.S_IWUSR | stat.S_IRGRP |
               stat.S_IWGRP | stat.S_IXGRP)
  # Install the driver.conf file
  with open(os.path.join(dstdir, 'driver.conf'), 'w') as f:
    print >> f, 'HAS_FRONTEND=1'
    print >> f, 'HOST_ARCH=x86_64' if host_64bit else 'HOST_ARCH=x86_32'
    for line in extra_config:
      print >> f, subst.Substitute(line)
  # Install the REV file
  with open(os.path.join(dstdir, 'REV'), 'w') as f:
    try:
      url, rev = repo_tools.GitRevInfo(NACL_DIR)
      repotype = 'GIT'
    except subprocess.CalledProcessError:
      url, rev = repo_tools.SvnRevInfo(NACL_DIR)
      repotype = 'SVN'
    print >> f, '[%s] %s: %s' % (repotype, url, rev)

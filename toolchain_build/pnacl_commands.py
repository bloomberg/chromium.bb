#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runnables for toolchain_build_pnacl.py
"""

import base64
import os
import shutil
import stat
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.file_tools
import pynacl.repo_tools


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

# User-facing tools
DRIVER_TOOLS = ['pnacl-' + tool + '.py' for tool in
                    ('abicheck', 'ar', 'as', 'clang', 'clang++', 'compress',
                     'dis', 'driver', 'finalize', 'ld', 'nm', 'opt',
                     'ranlib', 'readelf', 'strip', 'translate')]
# Utilities used by the driver
DRIVER_UTILS = [name + '.py' for name in
                    ('artools', 'driver_env', 'driver_log', 'driver_temps',
                     'driver_tools', 'elftools', 'filetype', 'ldtools',
                     'loader', 'nativeld', 'pathtools', 'shelltools')]

def InstallDriverScripts(subst, srcdir, dstdir, host_windows=False,
                         host_64bit=False, extra_config=[]):
  srcdir = subst.SubstituteAbsPaths(srcdir)
  dstdir = subst.SubstituteAbsPaths(dstdir)
  pynacl.file_tools.MakeDirectoryIfAbsent(os.path.join(dstdir, 'pydir'))
  for name in DRIVER_TOOLS + DRIVER_UTILS:
    shutil.copy(os.path.join(srcdir, name), os.path.join(dstdir, 'pydir'))
  # Install redirector sh/bat scripts
  for name in DRIVER_TOOLS:
    # Chop the .py off the name
    nopy_name = os.path.join(dstdir, os.path.splitext(name)[0])
    shutil.copy(os.path.join(srcdir, 'redirect.sh'), nopy_name)
    os.chmod(nopy_name,
             stat.S_IRUSR | stat.S_IXUSR | stat.S_IWUSR | stat.S_IRGRP |
             stat.S_IWGRP | stat.S_IXGRP)

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
      url, rev = pynacl.repo_tools.GitRevInfo(NACL_DIR)
      repotype = 'GIT'
    except subprocess.CalledProcessError:
      url, rev = pynacl.repo_tools.SvnRevInfo(NACL_DIR)
      repotype = 'SVN'
    print >> f, '[%s] %s: %s' % (repotype, url, rev)


def CheckoutGitBundleForTrybot(repo, destination):
  # For testing LLVM, Clang, etc. changes on the trybots, look for a
  # Git bundle file created by llvm_change_try_helper.sh.
  bundle_file = os.path.join(NACL_DIR, 'pnacl', 'not_for_commit',
                             '%s_bundle' % repo)
  base64_file = '%s.b64' % bundle_file
  if os.path.exists(base64_file):
    input_fh = open(base64_file, 'r')
    output_fh = open(bundle_file, 'wb')
    base64.decode(input_fh, output_fh)
    input_fh.close()
    output_fh.close()
    subprocess.check_call(
        pynacl.repo_tools.GitCmd() + ['fetch'],
        cwd=destination
    )
    subprocess.check_call(
        pynacl.repo_tools.GitCmd() + ['bundle', 'unbundle', bundle_file],
        cwd=destination
    )
    commit_id_file = os.path.join(NACL_DIR, 'pnacl', 'not_for_commit',
                                  '%s_commit_id' % repo)
    commit_id = open(commit_id_file, 'r').readline().strip()
    subprocess.check_call(
        pynacl.repo_tools.GitCmd() + ['checkout', commit_id],
        cwd=destination
    )

def CmdCheckoutGitBundleForTrybot(subst, repo, destination):
  return CheckoutGitBundleForTrybot(repo, subst.SubstituteAbsPaths(destination))

#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script will check out llvm and clang, and then package the results up
to a tgz file."""

import argparse
import fnmatch
import os
import shutil
import subprocess
import sys
import tarfile

# Path constants.
THIS_DIR = os.path.dirname(__file__)
LLVM_DIR = os.path.join(THIS_DIR, '..', '..', '..', 'third_party', 'llvm')
THIRD_PARTY_DIR = os.path.join(THIS_DIR, '..', '..', '..', 'third_party')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
LLVM_BUILD_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-build')
LLVM_BIN_DIR = os.path.join(LLVM_BUILD_DIR, 'Release+Asserts', 'bin')
LLVM_LIB_DIR = os.path.join(LLVM_BUILD_DIR, 'Release+Asserts', 'lib')
STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')


def Tee(output, logfile):
  logfile.write(output)
  print output,


def TeeCmd(cmd, logfile, fail_hard=True):
  """Runs cmd and writes the output to both stdout and logfile."""
  # Reading from PIPE can deadlock if one buffer is full but we wait on a
  # different one.  To work around this, pipe the subprocess's stderr to
  # its stdout buffer and don't give it a stdin.
  proc = subprocess.Popen(cmd, bufsize=1, stdin=open(os.devnull),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  for line in iter(proc.stdout.readline,''):
    Tee(line, logfile)
    if proc.poll() is not None:
      break
  exit_code = proc.wait()
  if exit_code != 0 and fail_hard:
    print 'Failed:', cmd
    sys.exit(1)


def PrintTarProgress(tarinfo):
  print 'Adding', tarinfo.name
  return tarinfo


def main():
  parser = argparse.ArgumentParser(description='build and package clang')
  parser.add_argument('--gcc-toolchain',
                      help="the prefix for the GCC version used for building. "
                           "For /opt/foo/bin/gcc, pass "
                           "'--gcc-toolchain '/opt/foo'")

  args = parser.parse_args()

  with open('buildlog.txt', 'w') as log:
    Tee('Diff in llvm:\n', log)
    TeeCmd(['svn', 'stat', LLVM_DIR], log, fail_hard=False)
    TeeCmd(['svn', 'diff', LLVM_DIR], log, fail_hard=False)
    Tee('Diff in llvm/tools/clang:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'tools', 'clang')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'tools', 'clang')],
           log, fail_hard=False)
    Tee('Diff in llvm/compiler-rt:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'compiler-rt')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'compiler-rt')],
           log, fail_hard=False)
    Tee('Diff in llvm/projects/libcxx:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'projects', 'libcxx')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'projects', 'libcxx')],
           log, fail_hard=False)
    Tee('Diff in llvm/projects/libcxxabi:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'projects', 'libcxxabi')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'projects', 'libcxxabi')],
           log, fail_hard=False)

    Tee('Starting build\n', log)

    # Do a clobber build.
    shutil.rmtree(LLVM_BOOTSTRAP_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BOOTSTRAP_INSTALL_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BUILD_DIR, ignore_errors=True)

    build_cmd = [sys.executable, os.path.join(THIS_DIR, 'update.py'),
                 '--bootstrap', '--force-local-build', '--run-tests',
                 '--no-stdin-hack']
    if args.gcc_toolchain is not None:
      build_cmd.append(args.gcc_toolchain)
    TeeCmd(build_cmd, log)

  stamp = open(STAMP_FILE).read().rstrip()
  pdir = 'clang-' + stamp
  print pdir
  shutil.rmtree(pdir, ignore_errors=True)
  os.makedirs(os.path.join(pdir, 'bin'))
  os.makedirs(os.path.join(pdir, 'lib'))

  golddir = 'llvmgold-' + stamp
  if sys.platform.startswith('linux'):
    try:            os.makedirs(os.path.join(golddir, 'lib'))
    except OSError: pass

  so_ext = 'dylib' if sys.platform == 'darwin' else 'so'

  # Copy buildlog over.
  shutil.copy('buildlog.txt', pdir)

  # Copy clang into pdir, symlink clang++ to it.
  shutil.copy(os.path.join(LLVM_BIN_DIR, 'clang'), os.path.join(pdir, 'bin'))
  os.symlink('clang', os.path.join(pdir, 'bin', 'clang++'))
  shutil.copy(os.path.join(LLVM_BIN_DIR, 'llvm-symbolizer'),
              os.path.join(pdir, 'bin'))
  if sys.platform == 'darwin':
    shutil.copy(os.path.join(LLVM_BIN_DIR, 'libc++.1.dylib'),
                os.path.join(pdir, 'bin'))
    os.symlink('libc++.1.dylib', os.path.join(pdir, 'bin', 'libc++.dylib'))

  # Copy libc++ headers.
  if sys.platform == 'darwin':
    shutil.copytree(os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'include', 'c++'),
                    os.path.join(pdir, 'include', 'c++'))

  # Copy plugins. Some of the dylibs are pretty big, so copy only the ones we
  # care about.
  shutil.copy(os.path.join(LLVM_LIB_DIR, 'libFindBadConstructs.' + so_ext),
              os.path.join(pdir, 'lib'))
  shutil.copy(os.path.join(LLVM_LIB_DIR, 'libBlinkGCPlugin.' + so_ext),
              os.path.join(pdir, 'lib'))

  # Copy gold plugin on Linux.
  if sys.platform.startswith('linux'):
    shutil.copy(os.path.join(LLVM_LIB_DIR, 'LLVMgold.so'),
                os.path.join(golddir, 'lib'))

  if args.gcc_toolchain is not None:
    # Copy the stdlibc++.so.6 we linked Clang against so it can run.
    shutil.copy(os.path.join(LLVM_LIB_DIR, 'libstdc++.so.6'),
                os.path.join(pdir, 'lib'))

  # Copy built-in headers (lib/clang/3.x.y/include).
  # compiler-rt builds all kinds of libraries, but we want only some.
  if sys.platform == 'darwin':
    # Keep only the OSX (ASan and profile) and iossim (ASan) runtime libraries:
    want = ['*/lib/darwin/*asan_osx*',
            '*/lib/darwin/*asan_iosim*',
            '*/lib/darwin/*profile_osx*']
    for root, dirs, files in os.walk(os.path.join(LLVM_LIB_DIR, 'clang')):
      for f in files:
        qualified = os.path.join(root, f)
        if not any(fnmatch.fnmatch(qualified, p) for p in want):
          os.remove(os.path.join(root, f))
        elif f.endswith('.dylib'):
          # Fix LC_ID_DYLIB for the ASan dynamic libraries to be relative to
          # @executable_path.
          # TODO(glider): this is transitional. We'll need to fix the dylib
          # name either in our build system, or in Clang. See also
          # http://crbug.com/344836.
          subprocess.call(['install_name_tool', '-id',
                           '@executable_path/' + f, qualified])
          subprocess.call(['strip', '-x', qualified])
  elif sys.platform.startswith('linux'):
    # Keep only
    # lib/clang/*/lib/linux/libclang_rt.{[atm]san,san,ubsan,profile}-*.a ,
    # but not dfsan.
    want = ['*/lib/linux/*[atm]san*',
            '*/lib/linux/*ubsan*',
            '*/lib/linux/*libclang_rt.san*',
            '*/lib/linux/*profile*']
    for root, dirs, files in os.walk(os.path.join(LLVM_LIB_DIR, 'clang')):
      for f in files:
        qualified = os.path.join(root, f)
        if not any(fnmatch.fnmatch(qualified, p) for p in want):
          os.remove(os.path.join(root, f))
        elif not f.endswith('.syms'):
          # Strip the debug info from the runtime libraries.
          subprocess.call(['strip', '-g', qualified])

  shutil.copytree(os.path.join(LLVM_LIB_DIR, 'clang'),
                  os.path.join(pdir, 'lib', 'clang'))

  tar_entries = ['bin', 'lib', 'buildlog.txt']
  if sys.platform == 'darwin':
    tar_entries += ['include']
  with tarfile.open(pdir + '.tgz', 'w:gz') as tar:
    for entry in tar_entries:
      tar.add(os.path.join(pdir, entry), arcname=entry, filter=PrintTarProgress)

  if sys.platform.startswith('linux'):
    with tarfile.open(golddir + '.tgz', 'w:gz') as tar:
      tar.add(os.path.join(golddir, 'lib'), arcname='lib',
              filter=PrintTarProgress)

  if sys.platform == 'darwin':
    platform = 'Mac'
  else:
    platform = 'Linux_x64'

  print 'To upload, run:'
  print ('gsutil cp -a public-read %s.tgz '
         'gs://chromium-browser-clang/%s/%s.tgz') % (pdir, platform, pdir)

  if sys.platform.startswith('linux'):
    print ('gsutil cp -a public-read %s.tgz '
           'gs://chromium-browser-clang/%s/%s.tgz') % (golddir, platform,
                                                       golddir)

  # FIXME: Warn if the file already exists on the server.


if __name__ == '__main__':
  sys.exit(main())

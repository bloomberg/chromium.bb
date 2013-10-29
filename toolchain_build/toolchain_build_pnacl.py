#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

# Done first to set up python module path
import toolchain_env

import logging
import os
import sys

import command
import file_tools
import platform_tools
import repo_tools
import toolchain_main

GIT_REVISIONS = {
    # These keys are named such that they match up with the existing directory
    # names used by gclient and build.sh
    'binutils': '1712e607754a0eaa507ec6f735997542c99dd0be',
    'clang': '3b1ef29c338890ce55a1990389d8e63f6be7da2c',
    'llvm': 'e87e8cd648035ac8e2cda19ce0cfd9d9a6e309fb',
    'gcc': '2933722d1fe2aeb27e0471972b4c693fcbf71fc1',
    'libcxx': '7456fbacf145b57404b741dc72bac397b12f8330',
    'libcxxabi': '19222a8f373a6db1b13c4308988fd9cf1c02cb1b',
    'nacl-newlib': 'c37e9aefa58db1b357e17fdc3754a474923250e1',
    'llvm-test-suite': '0d6d7c909e6f7cc83e2b85c11332e9347b73dfb9',
    'compiler-rt': '8fc63ab2a0e0ccca85b9c3cc515fd291cbbb96e9',
    }

GIT_REPOS = {
    'binutils': 'nacl-binutils.git',
    'clang': 'pnacl-clang.git',
    'llvm': 'pnacl-llvm.git',
    'gcc': 'pnacl-gcc.git',
    'libcxx': 'pnacl-libcxx.git',
    'libcxxabi': 'pnacl-libcxxabi.git',
    'nacl-newlib': 'nacl-newlib.git',
    'llvm-test-suite': 'pnacl-llvm-testsuite.git',
    'compiler-rt': 'pnacl-compiler-rt.git',
    }

GIT_BASE_URL = 'https://chromium.googlesource.com/native_client/'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

CONFIGURE_CMD = ['sh', '%(src)s/configure']
MAKE_PARALLEL_CMD = [ 'make', '-j%(cores)s']
MAKE_BINUTILS_EXTRA = []

if platform_tools.IsLinux():
  # TODO(dschuff): handle the 64+32-bit package on Linux, or get rid of it.
  cc = 'gcc -m32'
  cxx = 'g++ -m32'
  CONFIGURE_CMD += ['--build=i686-linux', 'CC=' + cc, 'CXX=' + cxx]
elif platform_tools.IsWindows():
  command.Command.use_cygwin = True
  CONFIGURE_CMD += ['--disable-nls']
  MAKE_PARALLEL_CMD += ['HAVE_LIBICONV=no']
  # Windows has issues with the intentionally-racy 'chew' target
  MAKE_BINUTILS_EXTRA = ['-j1']
MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(abs_output)s']

def HostTools():
  tools = {
      'binutils_pnacl': {
          'git_url': GIT_BASE_URL + '/nacl-binutils.git',
          'git_revision': GIT_REVISIONS['binutils'],
          'commands': [
              command.Command(CONFIGURE_CMD + [
                  '--prefix=',
                  '--disable-silent-rules',
                  '--target=arm-pc-nacl',
                  '--program-prefix=le32-nacl-',
                  '--enable-targets=arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl,' +
                  'mipsel-pc-nacl',
                  '--enable-deterministic-archives',
                  '--enable-shared=no',
                  '--enable-gold=default',
                  '--enable-ld=no',
                  '--enable-plugins',
                  '--without-gas',
                  '--without-zlib',
                  '--with-sysroot=/arm-pc-nacl']),
              command.Command(MAKE_PARALLEL_CMD + MAKE_BINUTILS_EXTRA),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip'])] +
              [command.RemoveDirectory(command.path.join('%(abs_output)s', dir))
               for dir in ('arm-pc-nacl', 'lib', 'lib32')]
          },
      }
  return tools

PACKAGES = HostTools()

# This is to replace build.sh's use of gclient, which will eliminate the issues
# with msys vs cygwin git checkouts, and make testing easier. Note that the new
# build scripts will not share source directories with build.sh.
def SyncPNaClRepos():
  print '@@@BUILD_STEP sync PNaCl repos@@@'
  sys.stdout.flush()
  for repo, revision in GIT_REVISIONS.iteritems():
    destination = os.path.join(NACL_DIR, 'pnacl', 'git', repo)
    # The windows bots currently have their checkouts from cygwin git.
    # This will cause them to get re-cloned with depot_tools git.
    # TODO(dschuff): remove this once all the bot slaves have run it once.
    if platform_tools.IsWindows() and '--trybot' in sys.argv:
      file_tools.RemoveDirectoryIfPresent(destination)
    # This replaces build.sh's newlib-nacl-headers-clean step by cleaning the
    # the newlib repo on checkout (while silently blowing away any local
    # changes). TODO(dschuff): find a better way to handle nacl newlib headers.
    is_newlib = repo == 'nacl-newlib'
    repo_tools.SyncGitRepo(
        GIT_BASE_URL + GIT_REPOS[repo],
        destination,
        revision,
        clean=is_newlib)

if __name__ == '__main__':
  # This sets the logging for gclient-alike repo sync. It will be overridden
  # by the package builder based on the command-line flags.
  logging.getLogger().setLevel(logging.DEBUG)
  SyncPNaClRepos()
  tb = toolchain_main.PackageBuilder(PACKAGES,
                                     ['--no-cache-results',
                                      '--no-use-cached-results'] +
                                     sys.argv[1:])
  tb.Main()

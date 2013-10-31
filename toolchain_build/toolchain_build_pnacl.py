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

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
# Use the argparse from third_party to ensure it's the same on all platorms
python_lib_dir = os.path.join(os.path.dirname(NACL_DIR), 'third_party',
                              'python_libs', 'argparse')
sys.path.insert(0, python_lib_dir)
import argparse

# For backward compatibility, these key names match the directory names
# previously used with gclient
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

GIT_DEPS_FILE = os.path.join(NACL_DIR, 'pnacl', 'COMPONENT_REVISIONS')

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

def HostTools(revisions):
  tools = {
      'binutils_pnacl': {
          'git_url': GIT_BASE_URL + '/nacl-binutils.git',
          'git_revision': revisions['binutils'],
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


def ParseComponentRevisionsFile(filename):
  ''' Parse a simple-format deps file, with fields of the form:
key=value
Keys should match the keys in GIT_REPOS above, which match the previous
directory names used by gclient (with the exception that '_' in the file is
replaced by '-' in the returned key name).
Values are the git hashes for each repo.
Empty lines or lines beginning with '#' are ignored.
This function returns a dictionary mapping the keys found in the file to their
values.
'''
  with open(filename) as f:
    deps = {}
    for line in f:
      stripped = line.strip()
      if stripped.startswith('#') or len(stripped) == 0:
        continue
      tokens = stripped.split('=')
      if len(tokens) != 2:
        raise Exception('Malformed component revisions file: ' + filename)
      deps[tokens[0].replace('_', '-')] = tokens[1]
  return deps

# This is to replace build.sh's use of gclient, which will eliminate the issues
# with msys vs cygwin git checkouts, and make testing easier. Note that the new
# build scripts will not share source directories with build.sh.
def SyncPNaClRepos(revisions):
  print '@@@BUILD_STEP sync PNaCl repos@@@'
  sys.stdout.flush()
  for repo, revision in revisions.iteritems():
    destination = os.path.join(NACL_DIR, 'pnacl', 'git', repo)
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
  parser = argparse.ArgumentParser()
  parser.add_argument('--sync-only', action='store_true')
  args, leftover_args = parser.parse_known_args()
  print args, leftover_args
  revisions = ParseComponentRevisionsFile(GIT_DEPS_FILE)
  SyncPNaClRepos(revisions)
  if args.sync_only:
    sys.exit(0)
  packages = HostTools(revisions)
  tb = toolchain_main.PackageBuilder(packages,
                                     ['--no-cache-results',
                                      '--no-use-cached-results'] +
                                     leftover_args)
  tb.Main()

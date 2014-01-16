#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl toolchain packages.

   Recipes consist of specially-structured dictionaries, with keys for package
   name, type, commands to execute, etc. The structure is documented in the
   PackageBuilder docstring in toolchain_main.py.

   The real entry plumbing and CLI flags are also in toolchain_main.py.
"""

# Done first to set up python module path
import toolchain_env

import fnmatch
import logging
import os
import re
import shutil
import stat
import sys

import command
import file_tools
import platform_tools
import pnacl_commands
import pnacl_targetlibs
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

# TODO(dschuff): Some of this cygwin/mingw logic duplicates stuff in command.py
# and the mechanism for switching between cygwin and mingw is bad.
# Path to the hermetic cygwin
BUILD_CROSS_MINGW = False
CYGWIN_PATH = os.path.join(NACL_DIR, 'cygwin')
# Path to the mingw cross-compiler libs on Ubuntu
CROSS_MINGW_LIBPATH = '/usr/lib/gcc/i686-w64-mingw32/4.6'
# Path to the native mingw compiler is up here to make it easy to change for
# local testing.
MINGW_PATH = os.environ.get('MINGW', r'd:\src\mingw-exp\mingw32')

ALL_ARCHES = ('x86-32', 'x86-64', 'arm', 'mips32')

MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(abs_output)s']

def NativeTriple():
  if platform_tools.IsWindows():
    if command.Runnable.use_cygwin:
      return 'i686-pc-cygwin'
    else:
      return 'i686-w64-mingw32'
  elif platform_tools.IsMacOS():
    return 'x86_64-apple-darwin'
  elif platform_tools.IsLinux():
    if platform_tools.Is64BitLinux():
      return 'x86_64-linux'
    else:
      return 'i686-linux'
  else:
    raise Exception('Unknown build architecture %s' % sys.platform)


def TripleIsWindows(t):
  return fnmatch.fnmatch(t, '*-mingw32*') or fnmatch.fnmatch(t, '*cygwin*')


def ConfigureHostArchFlags(host):
  configure_args = []
  extra_cc_args = []

  native = NativeTriple()
  is_cross = host != native
  if is_cross:
    if platform_tools.Is64BitLinux() and fnmatch.fnmatch(host, '*-linux*'):
      # 64 bit linux can build 32 bit linux binaries while still being a native
      # build for our purposes. But it's not what config.guess will yield, so
      # use --build to force it and make sure things build correctly.
      configure_args.append('--build=' + host)
      extra_cc_args = ['-m32']
    else:
      configure_args.append('--host=' + host)

  extra_cxx_args = list(extra_cc_args)

  compiler = {
      # For now we only do native builds for linux and mac
      'i686-linux': ('gcc', 'g++'), # treat 32-bit linux like a native build
      'x86_64-linux': ('gcc', 'g++'),
      # TODO(dschuff): switch to clang on mac
      'x86_64-apple-darwin': ('gcc', 'g++'),
      # Windows build should work for native and cross
      'i686-w64-mingw32': ('i686-w64-mingw32-gcc', 'i686-w64-mingw32-g++'),
      # TODO: add arm-hosted support
  }
  cc = compiler[host][0]
  cxx = compiler[host][1]

  if is_cross:
    # LLVM's linux->mingw cross build isn't right without this?
    configure_args.append('CC_FOR_BUILD=gcc')
  configure_args.append('CC=' + ' '.join([cc] + extra_cc_args))
  configure_args.append('CXX=' + ' '.join([cxx] + extra_cxx_args))

  if TripleIsWindows(host):
    # The i18n support brings in runtime dependencies on MinGW DLLs
    # that we don't want to have to distribute alongside our binaries.
    # So just disable it, and compiler messages will always be in US English.
    configure_args.append('--disable-nls')
  return configure_args

def MakeCommand(host):
  make_command = ['make']
  if not platform_tools.IsWindows() or command.Runnable.use_cygwin:
    # The make that ships with msys sometimes hangs when run with -j.
    # The ming32-make that comes with the compiler itself reportedly doesn't
    # have this problem, but it has issues with pathnames with LLVM's build.
    make_command.append('-j%(cores)s')

  if TripleIsWindows(host):
    # There appears to be nothing we can pass at top-level configure time
    # that will prevent the configure scripts from finding MinGW's libiconv
    # and using it.  We have to force this variable into the environment
    # of the sub-configure runs, which are run via make.
    make_command.append('HAVE_LIBICONV=no')
  return make_command


def CopyWindowsHostLibs(host):
  if not TripleIsWindows(host):
    return []
  if command.Runnable.use_cygwin:
    libs = ('cyggcc_s-1.dll', 'cygiconv-2.dll', 'cygwin1.dll', 'cygintl-8.dll',
            'cygstdc++-6.dll', 'cygz.dll')
    return [command.Copy(
                    os.path.join(CYGWIN_PATH, 'bin', lib),
                    os.path.join('%(output)s', 'bin', lib))
                for lib in libs]
  if platform_tools.IsWindows():
    lib_path = os.path.join(MINGW_PATH, 'bin')
    # The native minGW compiler uses winpthread, but the Ubuntu cross compiler
    # does not.
    libs = ('libgcc_s_sjlj-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')
  else:
    lib_path = os.path.join(CROSS_MINGW_LIBPATH)
    libs = ('libgcc_s_sjlj-1.dll', 'libstdc++-6.dll')
  return [command.Copy(
                  os.path.join(lib_path, lib),
                  os.path.join('%(output)s', 'bin', lib))
               for lib in libs]

def GetGitSyncCmdCallback(revisions):
  """Return a callback which returns the git sync command for a component.

     This allows all the revision information to be processed here while giving
     other modules like pnacl_targetlibs.py the ability to define their own
     source targets with minimal boilerplate.
  """
  def GetGitSyncCmd(component):
    return command.SyncGitRepo(GIT_BASE_URL + GIT_REPOS[component],
                             '%(output)s',
                             revisions[component])
  return GetGitSyncCmd

def HostToolsSources(GetGitSyncCmd):
  sources = {
      'binutils_pnacl_src': {
          'type': 'source',
          'output_dirname': 'binutils',
          'commands': [
              GetGitSyncCmd('binutils'),
          ],
      },
      # For some reason, the llvm build using --with-clang-srcdir chokes if the
      # clang source directory is named something other than 'clang', so don't
      # change output_dirname for clang.
      'clang_src': {
          'type': 'source',
          'output_dirname': 'clang',
          'commands': [
              GetGitSyncCmd('clang'),
          ],
      },
      'llvm_src': {
          'type': 'source',
          'output_dirname': 'llvm',
          'commands': [
              GetGitSyncCmd('llvm'),
          ],
      },
  }
  return sources


# Return the component name we'll use for a base component name and
# a host triple.  The component names cannot contain dashes or other
# non-identifier characters, because the names of the files uploaded
# to Google Storage are constrained.  GNU configuration tuples contain
# dashes, which we translate to underscores.
def Mangle(component_name, extra):
  return component_name + '_' + re.sub(r'[^A-Za-z0-9_/.]', '_', extra)

def HostTools(host):
  def H(component_name):
    return Mangle(component_name, host)
  tools = {
      H('binutils_pnacl'): {
          'dependencies': ['binutils_pnacl_src'],
          'type': 'build',
          'commands': [
              command.SkipForIncrementalCommand([
                  'sh',
                  '%(binutils_pnacl_src)s/configure'] +
                  ConfigureHostArchFlags(host) +
                  ['--prefix=',
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
              command.Command(MakeCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip'])] +
              [command.RemoveDirectory(command.path.join('%(abs_output)s', dir))
               for dir in ('arm-pc-nacl', 'lib', 'lib32')]
          },
      H('llvm'): {
          'dependencies': ['clang_src', 'llvm_src', 'binutils_pnacl_src'],
          'type': 'build',
          'commands': [
              command.SkipForIncrementalCommand([
                  'sh',
                  '%(llvm_src)s/configure'] +
                  ConfigureHostArchFlags(host) +
                  ['--prefix=/',
                   '--enable-shared',
                   '--disable-zlib',
                   '--disable-jit',
                   '--with-binutils-include=%(abs_binutils_pnacl_src)s/include',
                   '--enable-targets=x86,arm,mips',
                   '--program-prefix=',
                   '--enable-optimized',
                   '--with-clang-srcdir=%(abs_clang_src)s']),
              command.Command(MakeCommand(host) + [
                  'VERBOSE=1',
                  'NACL_SANDBOX=0',
                  'all']),
              command.Command(MAKE_DESTDIR_CMD + ['install'])] +
              CopyWindowsHostLibs(host),
      },
      H('driver'): {
        'type': 'build',
        'inputs': { 'src': os.path.join(NACL_DIR, 'pnacl', 'driver')},
        'commands': [
            command.Runnable(pnacl_commands.InstallDriverScripts,
                             '%(src)s', '%(output)s',
                             host_windows=TripleIsWindows(host),
                             host_64bit=fnmatch.fnmatch(host, 'x86_64*'))
        ],
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
  parser.add_argument('--legacy-repo-sync', action='store_true',
                      dest='legacy_repo_sync', default=False,
                      help='Sync the git repo directories used by build.sh')
  args, leftover_args = parser.parse_known_args()
  revisions = ParseComponentRevisionsFile(GIT_DEPS_FILE)
  if args.legacy_repo_sync:
    SyncPNaClRepos(revisions)
    sys.exit(0)

  packages = {}
  packages.update(HostToolsSources(GetGitSyncCmdCallback(revisions)))
  packages.update(pnacl_targetlibs.TargetLibsSrc(
      GetGitSyncCmdCallback(revisions)))


  if platform_tools.Is64BitLinux():
    hosts = ['i686-linux', NativeTriple()]
  else:
    hosts = [NativeTriple()]
  if platform_tools.IsLinux() and BUILD_CROSS_MINGW:
    hosts.append('i686-w64-mingw32')
  for host in hosts:
    packages.update(HostTools(host))

  tb = toolchain_main.PackageBuilder(packages,
                                     leftover_args)
  tb.Main()

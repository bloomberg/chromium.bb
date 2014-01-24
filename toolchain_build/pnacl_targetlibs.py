#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl target libs."""

# Done first to set up python module path
import toolchain_env

import os
import re
import sys

import command
import fnmatch
import gsd_storage
import platform_tools
import pnacl_commands

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

# Return the path to the local copy of the driver script.
# msys should be false if the path will be called directly rather than passed to
# an msys or cygwin tool such as sh or make.
def PnaclTool(toolname, msys=True):
  ext = '.bat' if not msys and platform_tools.IsWindows() else ''
  return command.path.join('%(cwd)s', 'driver', 'pnacl-' + toolname + ext)

# PNaCl tools for newlib's environment, e.g. CC_FOR_TARGET=/path/to/pnacl-clang
TOOL_ENV_NAMES = { 'CC': 'clang', 'CXX': 'clang++', 'AR': 'ar', 'NM': 'nm',
                   'RANLIB': 'ranlib', 'READELF': 'readelf',
                   'OBJDUMP': 'illegal', 'AS': 'illegal', 'LD': 'illegal',
                   'STRIP': 'illegal' }
TARGET_TOOLS = [ tool + '_FOR_TARGET=' + PnaclTool(name)
                 for tool, name in TOOL_ENV_NAMES.iteritems() ]

TARGET_BCLIB_CFLAGS = '-g -O2 -mllvm -inline-threshold=5'
NEWLIB_ISYSTEM_CFLAGS = ' '.join([
    '-isystem',
    command.path.join('%(abs_newlib)s', 'include')])
# HAS_THREAD_LOCAL is used by libc++abi's exception storage, the fallback is
# pthread otherwise.
LIBCXX_CFLAGS = ' '.join([TARGET_BCLIB_CFLAGS, NEWLIB_ISYSTEM_CFLAGS,
                         '-DHAS_THREAD_LOCAL=1'])
LIBSTDCXX_CFLAGS = ' '.join([TARGET_BCLIB_CFLAGS, NEWLIB_ISYSTEM_CFLAGS])

def MakeCommand():
  make_command = ['make']
  if not platform_tools.IsWindows() and not command.Runnable.use_cygwin:
    # The make that ships with msys sometimes hangs when run with -j.
    # The ming32-make that comes with the compiler itself reportedly doesn't
    # have this problem, but it has issues with pathnames with LLVM's build.
    make_command.append('-j%(cores)s')
  return make_command

# Return the component name to use for a component name with
# a host triple. GNU configuration triples contain dashes, which are converted
# to underscores so the names are legal for Google Storage.
def Mangle(component_name, extra):
  return component_name + '_' + gsd_storage.LegalizeName(extra)

def TripleIsWindows(t):
  return fnmatch.fnmatch(t, '*-mingw32*') or fnmatch.fnmatch(t, '*cygwin*')

# Copy the driver scripts to the working directory, with extra config to set
# paths to the compiled host binaries. This could also be done by injecting -B
# flags into all the target tool command lines, but the library builds don't
# support that for tools other than CC (i.e. there are CFLAGS but no overridable
# flags for binutils).
def CopyDriverForTargetLib(host):
  return [
      command.RemoveDirectory('driver'),
      command.Mkdir('driver'),
      command.Runnable(pnacl_commands.InstallDriverScripts,
                       '%(driver)s', '%(cwd)s/driver',
                       host_windows=TripleIsWindows(host),
                       host_64bit=False,
                       extra_config=['BPREFIXES=%(abs_' + Mangle('llvm', host) +
                        ')s %(abs_' + Mangle('binutils_pnacl', host) + ')s'])
      ]


def BuildTargetBitcodeCmd(source, output):
  return command.Command(
    [PnaclTool('clang', msys=False),
     '-Wall', '-Werror', '-O2', '-c', '-isystem', '%(newlib)s/include',
     command.path.join('%(src)s', source),
     '-o', command.path.join('%(output)s', output)])

def TargetLibsSrc(GitSyncCmd):
  newlib_sys_nacl = command.path.join('%(output)s',
                                      'newlib', 'libc', 'sys', 'nacl')
  source = {
      'newlib_src': {
          'type': 'source',
          'output_dirname': 'pnacl-newlib',
          'commands': [
              # Clean any headers exported from the NaCl tree before syncing.
              command.CleanGitWorkingDir(
                  '%(output)s', os.path.join('newlib', 'libc', 'include')),
              GitSyncCmd('nacl-newlib')] +
              # Remove newlib versions of headers that will be replaced by
              # headers from the NaCl tree.
              [command.RemoveDirectory(command.path.join(newlib_sys_nacl,
                                                         dirname))
               for dirname in ['bits', 'sys', 'machine']] + [
              command.Command([
                  sys.executable,
                  command.path.join('%(top_srcdir)s', 'src', 'trusted',
                                    'service_runtime', 'export_header.py'),
                  command.path.join('%(top_srcdir)s', 'src', 'trusted',
                                    'service_runtime', 'include'),
                  newlib_sys_nacl],
                  cwd='%(abs_output)s',
              )] + [
              command.Copy(
                  os.path.join('%(top_srcdir)s', 'src', 'untrusted', 'pthread',
                               header),
                  os.path.join('%(output)s', 'newlib', 'libc', 'include',
                               header))
              for header in ('pthread.h', 'semaphore.h')
       ]
      },
      'libcxx_src': {
          'type': 'source',
          'output_dirname': 'libcxx',
          'commands': [
              GitSyncCmd('libcxx'),
          ]
      },
      'libcxxabi_src': {
          'type': 'source',
          'output_dirname': 'libcxxabi',
          'commands': [
              GitSyncCmd('libcxxabi'),
          ]
      },
      'compiler_rt_src': {
          'type': 'source',
          'output_dirname': 'compiler-rt',
          'commands': [
              GitSyncCmd('compiler-rt'),
          ]
      },
      'gcc_src': {
          'type': 'source',
          'output_dirname': 'pnacl-gcc',
          'commands': [
              GitSyncCmd('gcc'),
          ]
      },
  }
  return source


def BitcodeLibs(host):
  def H(name):
    return Mangle(name, host)
  libs = {
      'newlib': {
          'type': 'build',
          'dependencies': [ 'newlib_src', H('llvm'),
                            H('binutils_pnacl'),],
          'inputs': { 'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands' :
              CopyDriverForTargetLib(host) + [
              command.SkipForIncrementalCommand(
                  ['sh', '%(newlib_src)s/configure'] +
                  TARGET_TOOLS +
                  ['CFLAGS_FOR_TARGET=' + TARGET_BCLIB_CFLAGS + ' -allow-asm',
                  '--disable-multilib',
                  '--prefix=',
                  '--disable-newlib-supplied-syscalls',
                  '--disable-texinfo',
                  '--disable-libgloss',
                  '--enable-newlib-iconv',
                  '--enable-newlib-iconv-from-encodings=' +
                  'UTF-8,UTF-16LE,UCS-4LE,UTF-16,UCS-4',
                  '--enable-newlib-iconv-to-encodings=' +
                  'UTF-8,UTF-16LE,UCS-4LE,UTF-16,UCS-4',
                  '--enable-newlib-io-long-long',
                  '--enable-newlib-io-long-double',
                  '--enable-newlib-io-c99-formats',
                  '--enable-newlib-mb',
                  '--target=le32-nacl'
              ]),
              command.Command(MakeCommand()),
              command.Command(['make', 'DESTDIR=%(abs_output)s', 'install']),
              command.CopyTree(command.path.join('%(output)s', 'le32-nacl',
                                                 'lib'),
                               command.path.join('%(output)s', 'lib')),
              command.CopyTree(
                  command.path.join('%(output)s','le32-nacl', 'include'),
                  command.path.join('%(output)s','include')),
              command.RemoveDirectory(command.path.join('%(output)s',
                                                        'le32-nacl')),
              command.Mkdir(os.path.join('%(output)s', 'include', 'nacl')),
              # Copy nacl_random.h, used by libc++. It uses the IRT, so should
              # be safe to include in the toolchain.
              command.Copy(os.path.join('%(top_srcdir)s', 'src', 'untrusted',
                                        'nacl', 'nacl_random.h'),
                           os.path.join('%(output)s', 'include', 'nacl',
                                        'nacl_random.h')),
          ],
      },
      'libcxx': {
          'type': 'build',
          'dependencies': ['libcxx_src', 'libcxxabi_src', 'llvm_src', 'gcc_src',
                           H('llvm'), H('binutils_pnacl'), 'newlib'],
          'inputs': { 'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands' :
              CopyDriverForTargetLib(host) + [
              command.SkipForIncrementalCommand(
                  ['cmake', '-G', 'Unix Makefiles',
                   '-DCMAKE_C_COMPILER_WORKS=1',
                   '-DCMAKE_CXX_COMPILER_WORKS=1',
                   '-DCMAKE_INSTALL_PREFIX=',
                   '-DCMAKE_BUILD_TYPE=Release',
                   '-DCMAKE_C_COMPILER=' + PnaclTool('clang'),
                   '-DCMAKE_CXX_COMPILER=' + PnaclTool('clang++'),
                   '-DCMAKE_AR=' + PnaclTool('ar'),
                   '-DCMAKE_NM=' + PnaclTool('nm'),
                   '-DCMAKE_RANLIB=' + PnaclTool('ranlib'),
                   '-DCMAKE_LD=' + PnaclTool('illegal'),
                   '-DCMAKE_AS=' + PnaclTool('illegal'),
                   '-DCMAKE_OBJDUMP=' + PnaclTool('illegal'),
                   '-DCMAKE_C_FLAGS=-std=gnu11 ' + LIBCXX_CFLAGS,
                   '-DCMAKE_CXX_FLAGS=-std=gnu++11 ' + LIBCXX_CFLAGS,
                   '-DLIT_EXECUTABLE=' + command.path.join(
                       '%(llvm_src)s', 'utils', 'lit', 'lit.py'),
                   '-DLLVM_LIT_ARGS=--verbose  --param shell_prefix="' +
                    os.path.join(NACL_DIR,'run.py') + '-arch env --retries=1" '+
                    '--param exe_suffix=".pexe" --param use_system_lib=true ' +
                    '--param link_flags="-std=gnu++11 --pnacl-exceptions=sjlj"',
                   '-DLIBCXX_ENABLE_CXX0X=0',
                   '-DLIBCXX_ENABLE_SHARED=0',
                   '-DLIBCXX_CXX_ABI=libcxxabi',
                   '-DLIBCXX_LIBCXXABI_INCLUDE_PATHS=' + command.path.join(
                       '%(abs_libcxxabi_src)s', 'include'),
                   '%(libcxx_src)s']),
              command.Copy(os.path.join('%(gcc_src)s', 'gcc',
                                        'unwind-generic.h'),
                           os.path.join('include', 'unwind.h')),
              command.Command(MakeCommand() + ['VERBOSE=1']),
              command.Command(['make', 'DESTDIR=%(abs_output)s', 'VERBOSE=1',
                               'install']),
          ],
      },
      'libstdcxx': {
          'type': 'build',
          'dependencies': ['gcc_src', 'gcc_src', H('llvm'), H('binutils_pnacl'),
                           'newlib'],
          'inputs': { 'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands' :
              CopyDriverForTargetLib(host) + [
              command.SkipForIncrementalCommand([
                  'sh',
                  command.path.join('%(gcc_src)s', 'libstdc++-v3',
                                    'configure')] +
                  TARGET_TOOLS + [
                  'CC_FOR_BUILD=cc',
                  'CC=' + PnaclTool('clang'),
                  'CXX=' + PnaclTool('clang++'),
                  'AR=' + PnaclTool('ar'),
                  'NM=' + PnaclTool('nm'),
                  'RAW_CXX_FOR_TARGET=' + PnaclTool('clang++'),
                  'LD=' + PnaclTool('illegal'),
                  'RANLIB=' + PnaclTool('ranlib'),
                  'CFLAGS=' + LIBSTDCXX_CFLAGS,
                  'CXXFLAGS=' + LIBSTDCXX_CFLAGS,
                  'CPPFLAGS=' + NEWLIB_ISYSTEM_CFLAGS,
                  'CFLAGS_FOR_TARGET=' + LIBSTDCXX_CFLAGS,
                  'CXXFLAGS_FOR_TARGET=' + LIBSTDCXX_CFLAGS,
                  '--host=arm-none-linux-gnueabi',
                  '--prefix=',
                  '--enable-cxx-flags=-D__SIZE_MAX__=4294967295',
                  '--disable-multilib',
                  '--disable-linux-futex',
                  '--disable-libstdcxx-time',
                  '--disable-sjlj-exceptions',
                  '--disable-libstdcxx-pch',
                  '--with-newlib',
                  '--disable-shared',
                  '--disable-rpath']),
              command.Copy(os.path.join('%(gcc_src)s', 'gcc',
                                        'unwind-generic.h'),
                           os.path.join('include', 'unwind.h')),
              command.Command(MakeCommand()),
              command.Command(['make', 'DESTDIR=%(abs_output)s',
                               'install-data']),
              command.RemoveDirectory(os.path.join('%(output)s', 'share')),
              command.Remove(os.path.join('%(output)s', 'lib',
                                          'libstdc++*-gdb.py')),
              command.Copy(os.path.join('src', '.libs', 'libstdc++.a'),
                           os.path.join('%(output)s', 'lib', 'libstdc++.a')),
          ],
      },
      'libs_support_bitcode': {
          'type': 'build',
          'dependencies': [ 'newlib', H('llvm'), H('binutils_pnacl')],
          'inputs': { 'src': os.path.join(NACL_DIR,
                                          'pnacl', 'support', 'bitcode'),
                      'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands':
              CopyDriverForTargetLib(host) + [
              # Two versions of crt1.x exist, for different scenarios (with and
              # without EH).  See:
              # https://code.google.com/p/nativeclient/issues/detail?id=3069
              command.Copy(command.path.join('%(src)s', 'crt1.x'),
                           command.path.join('%(output)s', 'crt1.x')),
              command.Copy(command.path.join('%(src)s', 'crt1_for_eh.x'),
                           command.path.join('%(output)s', 'crt1_for_eh.x')),
              # Install crti.bc (empty _init/_fini)
              BuildTargetBitcodeCmd('crti.c', 'crti.bc'),
              # Install crtbegin bitcode (__cxa_finalize for C++)
              BuildTargetBitcodeCmd('crtbegin.c', 'crtbegin.bc'),
              # Stubs for _Unwind_* functions when libgcc_eh is not included in
              # the native link).
              BuildTargetBitcodeCmd('unwind_stubs.c', 'unwind_stubs.bc'),
              BuildTargetBitcodeCmd('sjlj_eh_redirect.c',
                                    'sjlj_eh_redirect.bc'),
              # libpnaclmm.a (__atomic_* library functions).
              BuildTargetBitcodeCmd('pnaclmm.c', 'pnaclmm.bc'),
              command.Command([
                  PnaclTool('ar'), 'rc',
                  command.path.join('%(output)s', 'libpnaclmm.a'),
                  command.path.join('%(output)s', 'pnaclmm.bc')]),
          ],
      },
  }
  return libs

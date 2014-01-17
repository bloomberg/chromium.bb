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
  }
  return libs
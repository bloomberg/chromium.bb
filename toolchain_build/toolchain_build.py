#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import os
import sys

import command
import toolchain_main

GIT_REVISIONS = {
    'binutils': '0d99b0776661ebbf2949c644cb060612ccc11737',
    'gcc': 'aa98f0ce1ad49ac7faff1ef880a8016db520d333',
    'newlib': '51a8366a0898bc47ee78a7f6ed01e8bd40eee4d0',
    }

TARGET_LIST = ['arm']

# These are extra arguments to pass gcc's configure that vary by target.
TARGET_GCC_CONFIG = {
    'arm': ['--with-tune=cortex-a15'],
    }

PACKAGE_NAME = 'Native Client SDK [%(build_signature)s]'
BUG_URL = 'http://gonacl.com/reportissue'

GIT_BASE_URL = 'http://git.chromium.org/native_client'

TAR_XV = ['tar', '-x', '-v']
EXTRACT_STRIP_TGZ = TAR_XV + ['--gzip', '--strip-components=1', '-f']
EXTRACT_STRIP_TBZ2 = TAR_XV + ['--bzip2', '--strip-components=1', '-f']
CONFIGURE_CMD = ['%(input0)s/configure']
MAKE_PARALLEL_CMD = ['make', '-j%(cores)s']
MAKE_CHECK_CMD = MAKE_PARALLEL_CMD + ['check']
MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(output0)s']

CONFIGURE_HOST_ARCH = []
if sys.platform.startswith('linux'):
  # We build the tools for x86-32 hosts so they will run on either x86-32
  # or x86-64 hosts (with the right compatibility libraries installed).
  CONFIGURE_HOST_ARCH += [
      'CC=gcc -m32',
      'CXX=g++ -m32',
      '--build=i686-linux',
      ]

CONFIGURE_HOST_LIB = CONFIGURE_HOST_ARCH + [
      '--disable-shared',
      '--prefix=',
      ]

CONFIGURE_HOST_TOOL = CONFIGURE_HOST_LIB + [
    '--with-pkgversion=' + PACKAGE_NAME,
    '--with-bugurl=' + BUG_URL,
    '--without-zlib',
]

def ConfigureTargetArgs(arch):
  config_target = arch + '-nacl'
  return ['--target=' + config_target, '--with-sysroot=/' + config_target]

def CommandsInBuild(command_lines):
  return [command.Mkdir('build')] + [command.Command(cmd, cwd='build')
                                     for cmd in command_lines]

def UnpackSrc(is_gzip):
  if is_gzip:
    extract = EXTRACT_STRIP_TGZ
  else:
    extract = EXTRACT_STRIP_TBZ2
  return [
      command.Mkdir('src'),
      command.Command(extract + ['%(input0)s'], cwd='src'),
      ]

def PopulateDeps(dep_dirs):
  commands = [command.Mkdir('all_deps')]
  commands += [command.Command('cp -r "%s/"* all_deps' % dirname, shell=True)
               for dirname in dep_dirs]
  return commands

def WithDepsOptions(options):
  return ['--with-' + option + '=%(cwd)s/../all_deps' for option in options]

# These are libraries that go into building the compiler itself.
HOST_GCC_LIBS = {
    'gmp': {
        'tar_src': 'third_party/gmp/gmp-5.0.5.tar.bz2',
        'unpack_commands': UnpackSrc(False),
        'hashed_inputs': ['src'],
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + [
                '--with-sysroot=%(output0)s',
                '--enable-cxx',
                ],
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ]),
        },
    'mpfr': {
        'dependencies': ['gmp'],
        'tar_src': 'third_party/mpfr/mpfr-3.1.1.tar.bz2',
        'unpack_commands': UnpackSrc(False) + PopulateDeps(['%(input1)s']),
        'hashed_inputs': ['src', 'all_deps'],
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + WithDepsOptions(['sysroot',
                                                                  'gmp']),
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ])
        },
    'mpc': {
        'dependencies': ['gmp', 'mpfr'],
        'tar_src': 'third_party/mpc/mpc-1.0.tar.gz',
        'unpack_commands': UnpackSrc(True) + PopulateDeps(['%(input1)s',
                                                           '%(input2)s']),
        'hashed_inputs': ['src', 'all_deps'],
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + WithDepsOptions(['sysroot',
                                                                  'gmp',
                                                                  'mpfr']),
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ])
        },
    }

GCC_GIT_URL = GIT_BASE_URL + '/nacl-gcc.git'


def GccCommand(cmd):
  return command.Command(cmd, path_dirs=[os.path.join('%(input4)s', 'bin')])


def ConfigureGccCommand(target):
  return GccCommand(
      CONFIGURE_CMD +
      CONFIGURE_HOST_TOOL +
      ConfigureTargetArgs(target) +
      TARGET_GCC_CONFIG.get(target, []) + [
          '--with-gmp=%(input1)s',
          '--with-mpfr=%(input2)s',
          '--with-mpc=%(input3)s',
          '--disable-dlopen',
          '--with-newlib',
          '--with-linker-hash-style=gnu',
          '--enable-languages=c,c++,lto',
          ])


def HostTools(target):
  return {
      'binutils_' + target: {
          'git_url': GIT_BASE_URL + '/nacl-binutils.git',
          'git_revision': GIT_REVISIONS['binutils'],
          'commands': [
              command.Command(
                  CONFIGURE_CMD +
                  CONFIGURE_HOST_TOOL +
                  ConfigureTargetArgs(target) + [
                      '--enable-deterministic-archives',
                      '--enable-gold',
                      '--enable-plugins',
                      ]),
              command.Command(MAKE_PARALLEL_CMD),
              # TODO(mcgrathr): Run MAKE_CHECK_CMD here, but
              # check-ld has known failures for ARM targets.
              command.Command(MAKE_DESTDIR_CMD + ['install-strip'])
              # The top-level lib* directories contain host libraries
              # that we don't want to include in the distribution.
              ] + [command.RemoveDirectory(os.path.join('%(output0)s', name))
                   for name in ['lib', 'lib32', 'lib64']],
          },

      'gcc_' + target: {
          'dependencies': ['gmp', 'mpfr', 'mpc', 'binutils_' + target],
          'git_url': GCC_GIT_URL,
          'git_revision': GIT_REVISIONS['gcc'],
          # Remove all the source directories that are used solely for
          # building target libraries.  We don't want those included in the
          # input hash calculation so that we don't rebuild the compiler
          # when the the only things that have changed are target libraries.
          'unpack_commands': [command.RemoveDirectory(dirname) for dirname in [
                  'boehm-gc',
                  'libada',
                  'libffi',
                  'libgcc',
                  'libgfortran',
                  'libgo',
                  'libgomp',
                  'libitm',
                  'libjava',
                  'libmudflap',
                  'libobjc',
                  'libquadmath',
                  'libssp',
                  'libstdc++-v3',
                  ]],
          'commands': [
              ConfigureGccCommand(target),
              GccCommand(MAKE_PARALLEL_CMD + ['all-gcc']),
              GccCommand(MAKE_DESTDIR_CMD + ['install-strip-gcc']),
              ],
          },
      }


def CollectPackages(targets):
  packages = HOST_GCC_LIBS.copy()
  for target in targets:
    packages.update(HostTools(target))
    # TODO(mcgrathr): target libraries
  return packages

PACKAGES = CollectPackages(TARGET_LIST)


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  # TODO(mcgrathr): The bot ought to run some native_client tests
  # using the new toolchain, like the old x86 toolchain bots do.
  tb.Main()

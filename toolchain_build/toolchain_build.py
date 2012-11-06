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
    'gcc': '5f8b41df4ac38e009b4d66754cc728c848353ddb',
    'newlib': '51a8366a0898bc47ee78a7f6ed01e8bd40eee4d0',
    }

TARGET_LIST = ['arm']

# These are extra arguments to pass gcc's configure that vary by target.
TARGET_GCC_CONFIG = {
# TODO(mcgrathr): Disabled tuning for now, tickling a constant-pool layout bug.
#    'arm': ['--with-tune=cortex-a15'],
    }

PACKAGE_NAME = 'Native Client SDK [%(build_signature)s]'
BUG_URL = 'http://gonacl.com/reportissue'

GIT_BASE_URL = 'http://git.chromium.org/native_client'

TAR_XV = ['tar', '-x', '-v']
EXTRACT_STRIP_TGZ = TAR_XV + ['--gzip', '--strip-components=1', '-f']
EXTRACT_STRIP_TBZ2 = TAR_XV + ['--bzip2', '--strip-components=1', '-f']
CONFIGURE_CMD = ['sh', '%(src)s/configure']
MAKE_PARALLEL_CMD = ['make', '-j%(cores)s']
MAKE_CHECK_CMD = MAKE_PARALLEL_CMD + ['check']
MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(abs_output)s']

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


def NewlibLibcScript(arch):
  template = """/*
 * This is a linker script that gets installed as libc.a for the
 * newlib-based NaCl toolchain.  It brings in the constituent
 * libraries that make up what -lc means semantically.
 */
OUTPUT_FORMAT(%s)
GROUP ( libcrt_common.a libnacl.a )
"""
  if arch == 'arm':
    # Listing three formats instead of one makes -EL/-EB switches work
    # for the endian-switchable ARM backend.
    format_list = ['elf32-littlearm-nacl',
                   'elf32-bigarm-nacl',
                   'elf32-littlearm-nacl']
  else:
    raise Exception('TODO(mcgrathr): OUTPUT_FORMAT for %s' % arch)
  return template % ', '.join(['"' + fmt + '"' for fmt in format_list])


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
      command.Command(extract + ['%(src)s'], cwd='src'),
      ]

def PopulateDeps(dep_dirs):
  commands = [command.Mkdir('all_deps')]
  commands += [command.Command('cp -r "%s/"* all_deps' % dirname, shell=True)
               for dirname in dep_dirs]
  return commands

def WithDepsOptions(options):
  return ['--with-' + option + '=%(abs_all_deps)s' for option in options]

# These are libraries that go into building the compiler itself.
HOST_GCC_LIBS = {
    'gmp': {
        'tar_src': 'third_party/gmp/gmp-5.0.5.tar.bz2',
        'unpack_commands': UnpackSrc(False),
        'hashed_inputs': {'src': 'src'},
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + [
                '--with-sysroot=%(abs_output)s',
                '--enable-cxx',
                # Without this, the built library will assume the
                # instruction set details available on the build machine.
                # With this, it dynamically chooses what code to use based
                # on the details of the actual host CPU at runtime.
                '--enable-fat',
                ],
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ]),
        },
    'mpfr': {
        'dependencies': ['gmp'],
        'tar_src': 'third_party/mpfr/mpfr-3.1.1.tar.bz2',
        'unpack_commands': UnpackSrc(False) + PopulateDeps(['%(gmp)s']),
        'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
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
        'unpack_commands': UnpackSrc(True) + PopulateDeps(['%(gmp)s',
                                                           '%(mpfr)s']),
        'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
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

HOST_GCC_LIBS_DEPS = ['gmp', 'mpfr', 'mpc']

GCC_GIT_URL = GIT_BASE_URL + '/nacl-gcc.git'


def GccCommand(target, cmd):
  return command.Command(
      cmd, path_dirs=[os.path.join('%(abs_binutils_' + target + ')s', 'bin')])


def ConfigureGccCommand(target, extra_args=[]):
  return GccCommand(
      target,
      CONFIGURE_CMD +
      CONFIGURE_HOST_TOOL +
      ConfigureTargetArgs(target) +
      TARGET_GCC_CONFIG.get(target, []) + [
          '--with-gmp=%(abs_gmp)s',
          '--with-mpfr=%(abs_mpfr)s',
          '--with-mpc=%(abs_mpc)s',
          '--disable-dlopen',
          '--disable-shared',
          '--with-newlib',
          '--with-linker-hash-style=gnu',
          '--enable-languages=c,c++,lto',
          ] + extra_args)


def HostTools(target):
  tools = {
      'binutils_' + target: {
          'git_url': GIT_BASE_URL + '/nacl-binutils.git',
          'git_revision': GIT_REVISIONS['binutils'],
          'commands': [
              command.Command(
                  CONFIGURE_CMD +
                  CONFIGURE_HOST_TOOL +
                  ConfigureTargetArgs(target) + [
                      '--disable-shared',
                      '--enable-deterministic-archives',
                      ] + ([] if sys.platform == 'win32' else [
                          # TODO(mcgrathr): gold will build again on mingw32
                          # after some simple fixes get merged in.
                          '--enable-gold',
                          '--enable-plugins',
                          ])),
              command.Command(MAKE_PARALLEL_CMD),
              # TODO(mcgrathr): Run MAKE_CHECK_CMD here, but
              # check-ld has known failures for ARM targets.
              command.Command(MAKE_DESTDIR_CMD + ['install-strip'])
              # The top-level lib* directories contain host libraries
              # that we don't want to include in the distribution.
              ] +
              [command.RemoveDirectory(os.path.join('%(output)s', name))
               for name in ['lib', 'lib32', 'lib64']],
          },

      'gcc_' + target: {
          'dependencies': HOST_GCC_LIBS_DEPS + ['binutils_' + target],
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
              GccCommand(target, MAKE_PARALLEL_CMD + ['all-gcc']),
              GccCommand(target, MAKE_DESTDIR_CMD + ['install-strip-gcc']),
              ],
          },
      }
  return tools


def NewlibTargetCflags(target):
  if target == 'arm':
    tls_option = '-mtp=soft'
  else:
    tls_option = '-mtls-use-call'
  return ' '.join([
      '-O2',
      '-D_I386MACH_ALLOW_HW_INTERRUPTS',
      '-DSIGNAL_PROVIDED',
      tls_option,
      ])


def TargetCommands(target, command_list):
  # First we have to copy the host tools into a common directory.
  # We can't just have both directories in our PATH, because the
  # compiler looks for the assembler and linker relative to itself.
  commands = PopulateDeps(['%(binutils_' + target + ')s',
                           '%(gcc_' + target + ')s'])
  bindir = os.path.join('%(cwd)s', 'all_deps', 'bin')
  commands += [command.Command(cmd, path_dirs=[bindir])
               for cmd in command_list]
  return commands


def TargetLibs(target):
  lib_deps = ['binutils_' + target, 'gcc_' + target]

  # We have to populate the newlib source tree with the "exported" form of
  # some headers from the native_client source tree.  The newlib build
  # needs these to be in the expected place.  By doing this in the
  # 'unpack_commands' stage, these files will be part of the input hash and
  # so we don't need to do anything else to keep track of when they might
  # have changed in the native_client source tree.
  newlib_sys_nacl = os.path.join('%(src)s', 'newlib', 'libc', 'sys', 'nacl')
  newlib_unpack = [command.RemoveDirectory(os.path.join(newlib_sys_nacl,
                                                        dirname))
                   for dirname in ['bits', 'sys', 'machine']]
  newlib_unpack.append(command.Command([
      'python',
      os.path.join('%(top_srcdir)s',
                   'src', 'trusted', 'service_runtime', 'export_header.py'),
      os.path.join('%(top_srcdir)s',
                   'src', 'trusted', 'service_runtime', 'include'),
      newlib_sys_nacl,
      ]))

  def NewlibFile(subdir, name):
    return os.path.join('%(output)s', target + '-nacl', subdir, name)

  newlib_sysroot = '%(abs_newlib_' + target + ')s'
  newlib_tooldir = '%s/%s-nacl' % (newlib_sysroot, target)

  libs = {
      'newlib_' + target: {
          'dependencies': lib_deps,
          'git_url': GIT_BASE_URL + '/nacl-newlib.git',
          'git_revision': GIT_REVISIONS['newlib'],
          'unpack_commands': newlib_unpack,
          'commands': TargetCommands(target, [
              CONFIGURE_CMD +
              CONFIGURE_HOST_TOOL +
              ConfigureTargetArgs(target) + [
                  '--disable-libgloss',
                  '--enable-newlib-iconv',
                  '--enable-newlib-io-long-long',
                  '--enable-newlib-io-long-double',
                  '--enable-newlib-io-c99-formats',
                  '--enable-newlib-mb',
                  'CFLAGS=-O2',
                  'CFLAGS_FOR_TARGET=' + NewlibTargetCflags(target),
                  ],
              MAKE_PARALLEL_CMD,
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]) + [
                  command.Remove(NewlibFile('include', 'pthread.h')),
                  command.Rename(NewlibFile('lib', 'libc.a'),
                                 NewlibFile('lib', 'libcrt_common.a')),
                  command.WriteData(NewlibLibcScript(target),
                                    NewlibFile('lib', 'libc.a')),
                  ],
          },

      'gcc_libs_' + target: {
          'dependencies': lib_deps + ['newlib_' + target] + HOST_GCC_LIBS_DEPS,
          'git_url': GCC_GIT_URL,
          'git_revision': GIT_REVISIONS['gcc'],
          # This actually builds the compiler again and uses that compiler
          # to build the target libraries.  That's by far the easiest thing
          # to get going given the interdependencies of the target
          # libraries (especially libgcc) on the gcc subdirectory, and
          # building the compiler doesn't really take all that long in the
          # grand scheme of things.
          # TODO(mcgrathr): If upstream ever cleans up all their
          # interdependencies better, unpack the compiler, configure with
          # --disable-gcc, and just build all-target.
          'commands': [
              ConfigureGccCommand(target, [
                  '--with-build-sysroot=' + newlib_sysroot,
                  ]),
              GccCommand(target, MAKE_PARALLEL_CMD + [
                  'build_tooldir=' + newlib_tooldir,
                  'all-target',
                  ]),
              GccCommand(target, MAKE_DESTDIR_CMD + ['install-strip-target']),
              ],
          },
      }
  return libs


def CollectPackages(targets):
  packages = HOST_GCC_LIBS.copy()
  for target in targets:
    packages.update(HostTools(target))
    # TODO(mcgrathr): Eventually build target libraries on only one host type.
    packages.update(TargetLibs(target))
  return packages


PACKAGES = CollectPackages(TARGET_LIST)


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  # TODO(mcgrathr): The bot ought to run some native_client tests
  # using the new toolchain, like the old x86 toolchain bots do.
  tb.Main()

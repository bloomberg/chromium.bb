#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import os
import platform
import sys

import command
import toolchain_main

GIT_REVISIONS = {
    'binutils': '8c79023cd333c8be4276eefca7aa129b3db67d3e',# tag binutils-2_23_2
    'gcc': 'b90e7646d2f7d6c89a1143283b3c4e68ca17b9f6',
    'newlib': '5feee65e182c08a7e89fbffc3223c57e4335420f',
    }

TARGET_LIST = ['arm']

# These are extra arguments to pass gcc's configure that vary by target.
TARGET_GCC_CONFIG = {
# TODO(mcgrathr): Disabled tuning for now, tickling a constant-pool layout bug.
#    'arm': ['--with-tune=cortex-a15'],
    }

PACKAGE_NAME = 'Native Client SDK [%(build_signature)s]'
BUG_URL = 'http://gonacl.com/reportissue'

GIT_BASE_URL = 'https://chromium.googlesource.com/native_client'

TAR_XV = ['tar', '-x', '-v']
EXTRACT_STRIP_TGZ = TAR_XV + ['--gzip', '--strip-components=1', '-f']
EXTRACT_STRIP_TBZ2 = TAR_XV + ['--bzip2', '--strip-components=1', '-f']
CONFIGURE_CMD = ['sh', '%(src)s/configure']
MAKE_PARALLEL_CMD = ['make', '-j%(cores)s']
MAKE_CHECK_CMD = MAKE_PARALLEL_CMD + ['check']
MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(abs_output)s']

# This file gets installed by multiple packages' install steps, but it is
# never useful when installed in isolation.  So we remove it from the
# installation directories before packaging up.
REMOVE_INFO_DIR = command.Remove(os.path.join('%(output)s',
                                              'share', 'info', 'dir'))

CONFIGURE_HOST_ARCH = []
if sys.platform.startswith('linux'):
  cc = ['gcc']
  cxx = ['g++', '-static-libstdc++']
  if any(platform.machine().lower().startswith(machine) for machine in
         ['x86_64', 'amd64', 'x64', 'i686']):
    # We build the tools for x86-32 hosts so they will run on either x86-32
    # or x86-64 hosts (with the right compatibility libraries installed).
    cc += ['-m32']
    cxx += ['-m32']
    CONFIGURE_HOST_ARCH += ['--build=i686-linux']
  CONFIGURE_HOST_ARCH += [
      'CC=' + ' '.join(cc),
      'CXX=' + ' '.join(cxx),
      ]
elif sys.platform.startswith('win'):
  # The i18n support brings in runtime dependencies on MinGW DLLs
  # that we don't want to have to distribute alongside our binaries.
  # So just disable it, and compiler messages will always be in US English.
  CONFIGURE_HOST_ARCH += [
      '--disable-nls',
      ]
  # There appears to be nothing we can pass at top-level configure time
  # that will prevent the configure scripts from finding MinGW's libiconv
  # and using it.  We have to force this variable into the environment
  # of the sub-configure runs, which are run via make.
  MAKE_PARALLEL_CMD += [
      'HAVE_LIBICONV=no',
      ]

CONFIGURE_HOST_COMMON = CONFIGURE_HOST_ARCH + [
      '--prefix=',
      '--disable-silent-rules',
      '--without-gcc-arch',
      ]

CONFIGURE_HOST_LIB = CONFIGURE_HOST_COMMON + [
      '--disable-shared',
      ]

CONFIGURE_HOST_TOOL = CONFIGURE_HOST_COMMON + [
    '--with-pkgversion=' + PACKAGE_NAME,
    '--with-bugurl=' + BUG_URL,
    '--without-zlib',
]


def InstallDocFiles(subdir, files):
  doc_dir = os.path.join('%(output)s', 'share', 'doc', subdir)
  dirs = sorted(set([os.path.dirname(os.path.join(doc_dir, file))
                     for file in files]))
  commands = ([command.Mkdir(dir, parents=True) for dir in dirs] +
              [command.Copy(os.path.join('%(src)s', file),
                            os.path.join(doc_dir, file))
               for file in files])
  return commands


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


# The default strip behavior removes debugging and symbol table
# sections, but it leaves the .comment section.  This contains the
# compiler version string, and so it changes when the compiler changes
# even if the actual machine code it produces is completely identical.
# Hence, the target library packages will always change when the
# compiler changes unless these sections are removed.  Doing this
# requires somehow teaching the makefile rules to pass the
# --remove-section=.comment switch to TARGET-strip.  For the GCC
# target libraries, setting STRIP_FOR_TARGET is sufficient.  But
# quoting nightmares make it difficult to pass a command with a space
# in it as the STRIP_FOR_TARGET value.  So the build writes a little
# script that can be invoked with a simple name.
#
# Though the gcc target libraries' makefiles are smart enough to obey
# STRIP_FOR_TARGET for library files, the newlib makefiles just
# blindly use $(INSTALL_DATA) for both header (text) files and library
# files.  Hence it's necessary to override its INSTALL_DATA setting to
# one that will do stripping using this script, and thus the script
# must silently do nothing to non-binary files.
def ConfigureTargetPrep(arch):
  script_file = 'strip_for_target'

  config_target = arch + '-nacl'
  script_contents = """\
#!/bin/sh
mode=--strip-all
for arg; do
  case "$arg" in
  -*) ;;
  *)
    type=`file --brief --mime-type "$arg"`
    case "$type" in
      application/x-executable|application/x-sharedlib) ;;
      application/x-archive|application/x-object) mode=--strip-debug ;;
      *) exit 0 ;;
    esac
    ;;
  esac
done
exec %s-strip $mode --remove-section=.comment "$@"
""" % config_target

  return [
      command.WriteData(script_contents, script_file),
      command.Command(['chmod', '+x', script_file]),
      ]


def ConfigureTargetArgs(arch):
  config_target = arch + '-nacl'
  return [
      '--target=' + config_target,
      '--with-sysroot=/' + config_target,
      'STRIP_FOR_TARGET=%(cwd)s/strip_for_target',
      ]


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
        'tar_src': 'third_party/gmp/gmp-5.1.1.tar.bz2',
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
        'tar_src': 'third_party/mpfr/mpfr-3.1.2.tar.bz2',
        'unpack_commands': UnpackSrc(False) + PopulateDeps(['%(gmp)s']),
        'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + WithDepsOptions(['sysroot',
                                                                  'gmp']),
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ]),
        },
    'mpc': {
        'dependencies': ['gmp', 'mpfr'],
        'tar_src': 'third_party/mpc/mpc-1.0.1.tar.gz',
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
            ]),
        },
    'isl': {
        'dependencies': ['gmp'],
        'tar_src': 'third_party/cloog/isl-0.11.1.tar.bz2',
        'unpack_commands': UnpackSrc(False) + PopulateDeps(['%(gmp)s']),
        'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + WithDepsOptions([
                'sysroot',
                'gmp-prefix',
                ]),
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ]) + [
                # The .pc files wind up containing some absolute paths
                # that make the output depend on the build directory name.
                # The dependents' configure scripts don't need them anyway.
                command.RemoveDirectory(os.path.join('%(output)s',
                                                     'lib', 'pkgconfig')),
                ],
        },
    'cloog': {
        'dependencies': ['gmp', 'isl'],
        'tar_src': 'third_party/cloog/cloog-0.18.0.tar.gz',
        'unpack_commands': UnpackSrc(True) + PopulateDeps(['%(gmp)s',
                                                           '%(isl)s']),
        'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
        'commands': CommandsInBuild([
            CONFIGURE_CMD + CONFIGURE_HOST_LIB + [
                '--with-bits=gmp',
                '--with-isl=system',
                ] + WithDepsOptions(['sysroot',
                                     'gmp-prefix',
                                     'isl-prefix']),
            MAKE_PARALLEL_CMD,
            MAKE_CHECK_CMD,
            MAKE_DESTDIR_CMD + ['install-strip'],
            ]) + [
                # The .pc files wind up containing some absolute paths
                # that make the output depend on the build directory name.
                # The dependents' configure scripts don't need them anyway.
                command.RemoveDirectory(os.path.join('%(output)s',
                                                     'lib', 'pkgconfig')),
                ],
        },
    }

HOST_GCC_LIBS_DEPS = ['gmp', 'mpfr', 'mpc', 'isl', 'cloog']

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
          '--with-isl=%(abs_isl)s',
          '--with-cloog=%(abs_cloog)s',
          '--enable-cloog-backend=isl',
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
          'commands': ConfigureTargetPrep(target) + [
              command.Command(
                  CONFIGURE_CMD +
                  CONFIGURE_HOST_TOOL +
                  ConfigureTargetArgs(target) + [
                      '--enable-deterministic-archives',
                      '--enable-gold',
                      ] + ([] if sys.platform == 'win32' else [
                          '--enable-plugins',
                          ])),
              command.Command(MAKE_PARALLEL_CMD),
              command.Command(MAKE_CHECK_CMD),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              REMOVE_INFO_DIR,
              ] + InstallDocFiles('binutils',
                                  ['COPYING3'] +
                                  [os.path.join(subdir, 'NEWS') for subdir in
                                   ['binutils', 'gas', 'ld', 'gold']]) +
              # The top-level lib* directories contain host libraries
              # that we don't want to include in the distribution.
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
          'commands': ConfigureTargetPrep(target) + [
              ConfigureGccCommand(target),
              # gcc/Makefile's install rules ordinarily look at the
              # installed include directory for a limits.h to decide
              # whether the lib/gcc/.../include-fixed/limits.h header
              # should be made to expect a libc-supplied limits.h or not.
              # Since we're doing this build in a clean environment without
              # any libc installed, we need to force its hand here.
              GccCommand(target, MAKE_PARALLEL_CMD + ['all-gcc',
                                                      'LIMITS_H_TEST=true']),
              # gcc/Makefile's install targets populate this directory
              # only if it already exists.
              command.Mkdir(os.path.join('%(output)s', target + '-nacl', 'bin'),
                            True),
              GccCommand(target, MAKE_DESTDIR_CMD + ['install-strip-gcc']),
              REMOVE_INFO_DIR,
              # Note we include COPYING.RUNTIME here and not with gcc_libs.
              ] + InstallDocFiles('gcc', ['COPYING3', 'COPYING.RUNTIME']),
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

  # See the comment at ConfigureTargetPrep, above.
  newlib_install_data = ' '.join(['STRIPPROG=%(cwd)s/strip_for_target',
                                  '%(abs_src)s/install-sh',
                                  '-c', '-s', '-m', '644'])

  libs = {
      'newlib_' + target: {
          'dependencies': lib_deps,
          'git_url': GIT_BASE_URL + '/nacl-newlib.git',
          'git_revision': GIT_REVISIONS['newlib'],
          'unpack_commands': newlib_unpack,
          'commands': ConfigureTargetPrep(target) + TargetCommands(target, [
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
                  'INSTALL_DATA=' + newlib_install_data,
                  ],
              MAKE_PARALLEL_CMD,
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]) + [
                  command.Remove(NewlibFile('include', 'pthread.h')),
                  command.Rename(NewlibFile('lib', 'libc.a'),
                                 NewlibFile('lib', 'libcrt_common.a')),
                  command.WriteData(NewlibLibcScript(target),
                                    NewlibFile('lib', 'libc.a')),
                  ] + InstallDocFiles('newlib', ['COPYING.NEWLIB']),
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
          'commands': ConfigureTargetPrep(target) + [
              ConfigureGccCommand(target, [
                  '--with-build-sysroot=' + newlib_sysroot,
                  ]),
              GccCommand(target, MAKE_PARALLEL_CMD + [
                  'build_tooldir=' + newlib_tooldir,
                  'all-target',
                  ]),
              GccCommand(target, MAKE_DESTDIR_CMD + ['install-strip-target']),
              REMOVE_INFO_DIR,
              ],
          },
      }
  return libs


def CollectPackages(targets):
  packages = HOST_GCC_LIBS.copy()
  for target in targets:
    packages.update(HostTools(target))
    # We build target libraries only on Linux for two reasons:
    # 1. We only need to build them once.
    # 2. Linux is the fastest to build.
    # TODO(mcgrathr): In future set up some scheme whereby non-Linux
    # bots can build target libraries but not archive them, only verifying
    # that the results came out the same as the ones archived by the
    # official builder bot.  That will serve as a test of the host tools
    # on the other host platforms.
    if sys.platform.startswith('linux'):
      packages.update(TargetLibs(target))
  return packages


PACKAGES = CollectPackages(TARGET_LIST)


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  # TODO(mcgrathr): The bot ought to run some native_client tests
  # using the new toolchain, like the old x86 toolchain bots do.
  tb.Main()

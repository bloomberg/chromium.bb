#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import fnmatch
import platform
import os
import re
import sys

import command
import gsd_storage
import toolchain_main
import repo_tools

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.platform

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

# See command.GenerateGitPatches for the schema of entries in this dict.
# Additionally, each may contain a 'repo' key whose value is the name
# to use in place of the package name when calling GitUrl (below).
GIT_REVISIONS = {
    'binutils': {
        'rev': '38dbda270a4248ab5b7facc012b9c8d8527f6fb2',
        'upstream-branch': 'upstream/binutils-2_24-branch',
        'upstream-name': 'binutils-2.24',
        # This is tag binutils-2_24, but Gerrit won't let us push
        # non-annotated tags, and the upstream tag is not annotated.
        'upstream-base': '237df3fa4a1d939e6fd1af0c3e5029a25a137310',
        },
    'gcc': {
        'rev': 'a2e4a3140c035233409598487c56b76108f8c74d',
        'upstream-branch': 'upstream/gcc-4_8-branch',
        'upstream-name': 'gcc-4.8.2',
         # Upstream tag gcc-4_8_2-release:
        'upstream-base': '9bcca88e24e64d4e23636aafa3404088b13bcb0e',
        },
    'newlib': {
        'rev': 'a9ae3c60b36dea3d8a10e18b1b6db952d21268c2',
        'upstream-branch': 'upstream/master',
        'upstream-name': 'newlib-2.0.0',
        # Upstream tag newlib_2_0_0:
        'upstream-base': 'c3fc84e062cacc2b3e13c1f6b9151d0cc85392ba',
        },
    'gdb': {
        'rev': '41afdf9d511d95073c4403c838d8ccba582d8eed',
        'repo': 'binutils',
        'upstream-branch': 'upstream/gdb-7.7-branch',
        'upstream-name': 'gdb-7.7',
        # Upstream tag gdb-7.7-release:
        'upstream-base': 'fe284cd86ba9761655a9281fef470d364e27eb85',
        },
    }

TAR_FILES = {
    'gmp': command.path.join('gmp', 'gmp-5.1.3.tar.bz2'),
    'mpfr': command.path.join('mpfr', 'mpfr-3.1.2.tar.bz2'),
    'mpc': command.path.join('mpc', 'mpc-1.0.2.tar.gz'),
    'isl': command.path.join('cloog', 'isl-0.12.2.tar.bz2'),
    'cloog': command.path.join('cloog', 'cloog-0.18.1.tar.gz'),
    'expat': command.path.join('expat', 'expat-2.0.1.tar.gz'),
    }

GIT_BASE_URL = 'https://chromium.googlesource.com/native_client'


def GitUrl(package):
  repo = GIT_REVISIONS[package].get('repo', package)
  return '%s/nacl-%s.git' % (GIT_BASE_URL, repo)


def CollectSources():
  sources = {}

  for package in TAR_FILES:
    tar_file = TAR_FILES[package]
    if fnmatch.fnmatch(tar_file, '*.bz2'):
      extract = EXTRACT_STRIP_TBZ2
    elif fnmatch.fnmatch(tar_file, '*.gz'):
      extract = EXTRACT_STRIP_TGZ
    else:
      raise Exception('unexpected file name pattern in TAR_FILES[%r]' % package)
    sources[package] = {
        'type': 'source',
        'commands': [
            command.Command(extract + [command.path.join('%(abs_top_srcdir)s',
                                                         '..', 'third_party',
                                                         tar_file)],
                            cwd='%(output)s'),
            ],
        }

  patch_packages = []
  patch_commands = []
  for package, info in GIT_REVISIONS.iteritems():
    sources[package] = {
        'type': 'source',
        'commands': [command.SyncGitRepo(GitUrl(package), '%(output)s',
                                         info['rev'])],
        }
    patch_packages.append(package)
    patch_commands.append(
        command.GenerateGitPatches('%(' + package + ')s/.git', info))

  sources['patches'] = {
      'type': 'build',
      'dependencies': patch_packages,
      'commands': patch_commands,
      }

  # The gcc_libs component gets the whole GCC source tree.
  sources['gcc_libs'] = sources['gcc']

  # The gcc component omits all the source directories that are used solely
  # for building target libraries.  We don't want those included in the
  # input hash calculation so that we don't rebuild the compiler when the
  # the only things that have changed are target libraries.
  sources['gcc'] = {
        'type': 'source',
        'dependencies': ['gcc_libs'],
        'commands': [command.CopyTree('%(gcc_libs)s', '%(output)s', [
            'boehm-gc',
            'libada',
            'libatomic',
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
            'libsanitizer',
            'libssp',
            'libstdc++-v3',
            ])]
      }

  # We have to populate the newlib source tree with the "exported" form of
  # some headers from the native_client source tree.  The newlib build
  # needs these to be in the expected place.  By doing this in the source
  # target, these files will be part of the input hash and so we don't need
  # to do anything else to keep track of when they might have changed in
  # the native_client source tree.
  newlib_sys_nacl = command.path.join('%(output)s',
                                      'newlib', 'libc', 'sys', 'nacl')
  newlib_unpack = [command.RemoveDirectory(command.path.join(newlib_sys_nacl,
                                                             dirname))
                   for dirname in ['bits', 'sys', 'machine']]
  newlib_unpack.append(command.Command([
      'python',
      command.path.join('%(top_srcdir)s', 'src',
                        'trusted', 'service_runtime', 'export_header.py'),
      command.path.join('%(top_srcdir)s', 'src',
                        'trusted', 'service_runtime', 'include'),
      newlib_sys_nacl,
      ]))
  sources['newlib']['commands'] += newlib_unpack

  return sources


# Canonical tuples we use for hosts.
WINDOWS_HOST_TUPLE = pynacl.platform.PlatformTriple('win', 'x86-64')
MAC_HOST_TUPLE = pynacl.platform.PlatformTriple('darwin', 'x86-64')
ARM_HOST_TUPLE = pynacl.platform.PlatformTriple('linux', 'arm')
LINUX_X86_32_TUPLE = pynacl.platform.PlatformTriple('linux', 'x86-32')
LINUX_X86_64_TUPLE = pynacl.platform.PlatformTriple('linux', 'x86-64')

# Map of native host tuple to extra tuples that it cross-builds for.
EXTRA_HOSTS_MAP = {
    LINUX_X86_64_TUPLE: [
        LINUX_X86_32_TUPLE,
        ARM_HOST_TUPLE,
        WINDOWS_HOST_TUPLE,
        ],
    }

# Map of native host tuple to host tuples that are "native enough".
# For these hosts, we will do a native-style build even though it's
# not the native tuple, just passing some extra compiler flags.
NATIVE_ENOUGH_MAP = {
    LINUX_X86_64_TUPLE: {
        LINUX_X86_32_TUPLE: ['-m32'],
        },
    }


# The list of targets to build toolchains for.
TARGET_LIST = ['arm', 'i686']

# These are extra arguments to pass gcc's configure that vary by target.
TARGET_GCC_CONFIG = {
# TODO(mcgrathr): Disabled tuning for now, tickling a constant-pool layout bug.
#    'arm': ['--with-tune=cortex-a15'],
    }

PACKAGE_NAME = 'Native Client SDK [%(build_signature)s]'
BUG_URL = 'http://gonacl.com/reportissue'

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
REMOVE_INFO_DIR = command.Remove(command.path.join('%(output)s',
                                                   'share', 'info', 'dir'))

def ConfigureHostArch(host):
  configure_args = []

  is_cross = CrossCompiling(host)

  if is_cross:
    extra_cc_args = []
    configure_args.append('--host=' + host)
  else:
    extra_cc_args = NATIVE_ENOUGH_MAP.get(NATIVE_TUPLE, {}).get(host, [])
    if extra_cc_args:
      # The host we've chosen is "native enough", such as x86-32 on x86-64.
      # But it's not what config.guess will yield, so we need to supply
      # a --build switch to ensure things build correctly.
      configure_args.append('--build=' + host)

  extra_cxx_args = list(extra_cc_args)
  if fnmatch.fnmatch(host, '*-linux*'):
    # Avoid shipping binaries with a runtime dependency on
    # a particular version of the libstdc++ shared library.
    # TODO(mcgrathr): Do we want this for MinGW and/or Mac too?
    extra_cxx_args.append('-static-libstdc++')

  if extra_cc_args:
    # These are the defaults when there is no setting, but we will add
    # additional switches, so we must supply the command name too.
    if is_cross:
      cc = host + '-gcc'
    else:
      cc = 'gcc'
    configure_args.append('CC=' + ' '.join([cc] + extra_cc_args))

  if extra_cxx_args:
    # These are the defaults when there is no setting, but we will add
    # additional switches, so we must supply the command name too.
    if is_cross:
      cxx = host + '-g++'
    else:
      cxx = 'g++'
    configure_args.append('CXX=' + ' '.join([cxx] + extra_cxx_args))

  if HostIsWindows(host):
    # The i18n support brings in runtime dependencies on MinGW DLLs
    # that we don't want to have to distribute alongside our binaries.
    # So just disable it, and compiler messages will always be in US English.
    configure_args.append('--disable-nls')

  return configure_args


def ConfigureHostCommon(host):
  return ConfigureHostArch(host) + [
      '--prefix=',
      '--disable-silent-rules',
      '--without-gcc-arch',
      ]


def ConfigureHostLib(host):
  return ConfigureHostCommon(host) + [
      '--disable-shared',
      ]


def ConfigureHostTool(host):
  return ConfigureHostCommon(host) + [
      '--with-pkgversion=' + PACKAGE_NAME,
      '--with-bugurl=' + BUG_URL,
      '--without-zlib',
      ]


def MakeCommand(host, extra_args=[]):
  if HostIsWindows(host):
    # There appears to be nothing we can pass at top-level configure time
    # that will prevent the configure scripts from finding MinGW's libiconv
    # and using it.  We have to force this variable into the environment
    # of the sub-configure runs, which are run via make.
    make_command = MAKE_PARALLEL_CMD + ['HAVE_LIBICONV=no']
  else:
    make_command = MAKE_PARALLEL_CMD
  return make_command + extra_args


# Return the 'make check' command to run.
# When cross-compiling, don't try to run test suites.
def MakeCheckCommand(host):
  if CrossCompiling(host):
    return ['true']
  return MAKE_CHECK_CMD


def InstallDocFiles(subdir, files):
  doc_dir = command.path.join('%(output)s', 'share', 'doc', subdir)
  dirs = sorted(set([command.path.dirname(command.path.join(doc_dir, file))
                     for file in files]))
  commands = ([command.Mkdir(dir, parents=True) for dir in dirs] +
              [command.Copy(command.path.join('%(' + subdir + ')s', file),
                            command.path.join(doc_dir, file))
               for file in files])
  return commands


def NewlibLibcScript(arch):
  template = """/*
 * This is a linker script that gets installed as libc.a for the
 * newlib-based NaCl toolchain.  It brings in the constituent
 * libraries that make up what -lc means semantically.
 */
OUTPUT_FORMAT(%s)
GROUP ( libnacl.a libcrt_common.a )
"""
  if arch == 'arm':
    # Listing three formats instead of one makes -EL/-EB switches work
    # for the endian-switchable ARM backend.
    format_list = ['elf32-littlearm-nacl',
                   'elf32-bigarm-nacl',
                   'elf32-littlearm-nacl']
  elif arch == 'i686':
    format_list = 'elf32-i386-nacl'
  elif arch == 'x86_64':
    format_list = 'elf32-x86_64-nacl'
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
  return [
      command.RemoveDirectory('build'),
      command.Mkdir('build'),
      ] + [command.Command(cmd, cwd='build')
           for cmd in command_lines]


def PopulateDeps(dep_dirs):
  commands = [command.RemoveDirectory('all_deps'),
              command.Mkdir('all_deps')]
  commands += [command.Command('cp -r "%s/"* all_deps' % dirname, shell=True)
               for dirname in dep_dirs]
  return commands


def WithDepsOptions(options, component=None):
  if component is None:
    directory = command.path.join('%(cwd)s', 'all_deps')
  else:
    directory = '%(abs_' + component + ')s'
  return ['--with-' + option + '=' + directory
          for option in options]


# Return the component name we'll use for a base component name and
# a host tuple.  The component names cannot contain dashes or other
# non-identifier characters, because the names of the files uploaded
# to Google Storage are constrained.  GNU configuration tuples contain
# dashes, which we translate to underscores.
def ForHost(component_name, host):
  return component_name + '_' + gsd_storage.LegalizeName(host)


# These are libraries that go into building the compiler itself.
def HostGccLibs(host):
  def H(component_name):
    return ForHost(component_name, host)
  host_gcc_libs = {
      H('gmp'): {
          'type': 'build',
          'dependencies': ['gmp'],
          'commands': [
              command.Command(ConfigureCommand('gmp') +
                              ConfigureHostLib(host) + [
                                  '--with-sysroot=%(abs_output)s',
                                  '--enable-cxx',
                                  # Without this, the built library will
                                  # assume the instruction set details
                                  # available on the build machine.  With
                                  # this, it dynamically chooses what code
                                  # to use based on the details of the
                                  # actual host CPU at runtime.
                                  '--enable-fat',
                                  ]),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              ],
          },
      H('mpfr'): {
          'type': 'build',
          'dependencies': ['mpfr', H('gmp')],
          'commands': [
              command.Command(ConfigureCommand('mpfr') +
                              ConfigureHostLib(host) +
                              WithDepsOptions(['sysroot', 'gmp'], H('gmp'))),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              ],
          },
      H('mpc'): {
          'type': 'build',
          'dependencies': ['mpc', H('gmp'), H('mpfr')],
          'commands': PopulateDeps(['%(' + H('gmp') + ')s',
                                    '%(' + H('mpfr') + ')s']) + [
              command.Command(ConfigureCommand('mpc') +
                              ConfigureHostLib(host) +
                              WithDepsOptions(['sysroot', 'gmp', 'mpfr'])),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              ],
          },
      H('isl'): {
          'type': 'build',
          'dependencies': ['isl', H('gmp')],
          'commands': [
              command.Command(ConfigureCommand('isl') +
                              ConfigureHostLib(host) +
                              WithDepsOptions(['sysroot', 'gmp-prefix'],
                                              H('gmp'))),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              # The .pc files wind up containing some absolute paths
              # that make the output depend on the build directory name.
              # The dependents' configure scripts don't need them anyway.
              command.RemoveDirectory(command.path.join(
                  '%(output)s', 'lib', 'pkgconfig')),
              ],
          },
      H('cloog'): {
          'type': 'build',
          'dependencies': ['cloog', H('gmp'), H('isl')],
          'commands': PopulateDeps(['%(' + H('gmp') + ')s',
                                    '%(' + H('isl') + ')s']) + [
              command.Command(ConfigureCommand('cloog') +
                              ConfigureHostLib(host) + [
                                  '--with-bits=gmp',
                                  '--with-isl=system',
                                  ] + WithDepsOptions(['sysroot',
                                                       'gmp-prefix',
                                                       'isl-prefix'])),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              # The .pc files wind up containing some absolute paths
              # that make the output depend on the build directory name.
              # The dependents' configure scripts don't need them anyway.
              command.RemoveDirectory(command.path.join(
                  '%(output)s', 'lib', 'pkgconfig')),
              ],
          },
      H('expat'): {
          'type': 'build',
          'dependencies': ['expat'],
          'commands': [
              command.Command(ConfigureCommand('expat') +
                              ConfigureHostLib(host)),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + [
                  # expat does not support the install-strip target.
                  'installlib',
                  'INSTALL=%(expat)s/conftools/install-sh -c -s',
                  'INSTALL_DATA=%(expat)s/conftools/install-sh -c -m 644',
                  ]),
              ],
          },
      }
  return host_gcc_libs


HOST_GCC_LIBS_DEPS = ['gmp', 'mpfr', 'mpc', 'isl', 'cloog']

def HostGccLibsDeps(host):
  return [ForHost(package, host) for package in HOST_GCC_LIBS_DEPS]


def ConfigureCommand(source_component):
  return [command % {'src': '%(' + source_component + ')s'}
          for command in CONFIGURE_CMD]


# When doing a Canadian cross, we need native-hosted cross components
# to do the GCC build.
def GccDeps(host, target):
  components = ['binutils_' + target]
  if CrossCompiling(host):
    components.append('gcc_' + target)
    host = NATIVE_TUPLE
  return [ForHost(component, host) for component in components]


def GccCommand(host, target, cmd):
  components_for_path = GccDeps(host, target)
  return command.Command(
      cmd, path_dirs=[command.path.join('%(abs_' + component + ')s', 'bin')
                      for component in components_for_path])


def ConfigureGccCommand(source_component, host, target, extra_args=[]):
  return GccCommand(
      host,
      target,
      ConfigureCommand(source_component) +
      ConfigureHostTool(host) +
      ConfigureTargetArgs(target) +
      TARGET_GCC_CONFIG.get(target, []) + [
          '--with-gmp=%(abs_' + ForHost('gmp', host) + ')s',
          '--with-mpfr=%(abs_' + ForHost('mpfr', host) + ')s',
          '--with-mpc=%(abs_' + ForHost('mpc', host) + ')s',
          '--with-isl=%(abs_' + ForHost('isl', host) + ')s',
          '--with-cloog=%(abs_' + ForHost('cloog', host) + ')s',
          '--enable-cloog-backend=isl',
          '--disable-dlopen',
          '--disable-shared',
          '--with-newlib',
          '--with-linker-hash-style=gnu',
          '--enable-linker-build-id',
          '--enable-languages=c,c++,lto',
          ] + extra_args)



def HostTools(host, target):
  def H(component_name):
    return ForHost(component_name, host)
  tools = {
      H('binutils_' + target): {
          'type': 'build',
          'dependencies': ['binutils'],
          'commands': ConfigureTargetPrep(target) + [
              command.Command(
                  ConfigureCommand('binutils') +
                  ConfigureHostTool(host) +
                  ConfigureTargetArgs(target) + [
                      '--enable-deterministic-archives',
                      '--enable-gold',
                      ] + ([] if HostIsWindows(host) else [
                          '--enable-plugins',
                          ])),
              command.Command(MakeCommand(host)),
              command.Command(MakeCheckCommand(host)),
              command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
              REMOVE_INFO_DIR,
              ] + InstallDocFiles('binutils',
                                  ['COPYING3'] +
                                  [command.path.join(subdir, 'NEWS')
                                   for subdir in
                                   ['binutils', 'gas', 'ld', 'gold']]) +
              # The top-level lib* directories contain host libraries
              # that we don't want to include in the distribution.
              [command.RemoveDirectory(command.path.join('%(output)s', name))
               for name in ['lib', 'lib32', 'lib64']],
          },

      H('gcc_' + target): {
          'type': 'build',
          'dependencies': (['gcc'] + HostGccLibsDeps(host) +
                           GccDeps(host, target)),
          'commands': ConfigureTargetPrep(target) + [
              ConfigureGccCommand('gcc', host, target),
              # GCC's configure step writes configargs.h with some strings
              # including the configure command line, which get embedded
              # into the gcc driver binary.  The build only works if we use
              # absolute paths in some of the configure switches, but
              # embedding those paths makes the output differ in repeated
              # builds done in different directories, which we do not want.
              # So force the generation of that file early and then edit it
              # in place to replace the absolute paths with something that
              # never varies.  Note that the 'configure-gcc' target will
              # actually build some components before running gcc/configure.
              GccCommand(host, target,
                         MakeCommand(host, ['configure-gcc'])),
              command.Command(['sed', '-i', '-e',
                               ';'.join(['s@%%(abs_%s)s@.../%s_install@g' %
                                         (component, component)
                                         for component in
                                         HostGccLibsDeps(host)] +
                                        ['s@%(cwd)s@...@g']),
                               command.path.join('gcc', 'configargs.h')]),
              # gcc/Makefile's install rules ordinarily look at the
              # installed include directory for a limits.h to decide
              # whether the lib/gcc/.../include-fixed/limits.h header
              # should be made to expect a libc-supplied limits.h or not.
              # Since we're doing this build in a clean environment without
              # any libc installed, we need to force its hand here.
              GccCommand(host, target,
                         MakeCommand(host, ['all-gcc', 'LIMITS_H_TEST=true'])),
              # gcc/Makefile's install targets populate this directory
              # only if it already exists.
              command.Mkdir(command.path.join('%(output)s',
                                              target + '-nacl', 'bin'),
                            True),
              GccCommand(host, target,
                         MAKE_DESTDIR_CMD + ['install-strip-gcc']),
              REMOVE_INFO_DIR,
              # Note we include COPYING.RUNTIME here and not with gcc_libs.
              ] + InstallDocFiles('gcc', ['COPYING3', 'COPYING.RUNTIME']),
          },

      # GDB can support all the targets in one host tool.
      H('gdb'): {
          'type': 'build',
          'dependencies': ['gdb', H('expat')],
          'commands': [
              command.Command(
                  ConfigureCommand('gdb') +
                  ConfigureHostTool(host) + [
                      '--target=x86_64-nacl',
                      '--enable-targets=arm-none-eabi-nacl',
                      '--with-expat',
                      'CPPFLAGS=-I%(abs_' + H('expat') + ')s/include',
                      'LDFLAGS=-L%(abs_' + H('expat') + ')s/lib',
                      ] +
                  (['--without-python'] if HostIsWindows(host) else []) +
                  # TODO(mcgrathr): The default -Werror only breaks because
                  # the OSX default compiler is an old front-end that does
                  # not understand all the GCC options.  Maybe switch to
                  # using clang (system or Chromium-supplied) on Mac.
                  (['--disable-werror'] if HostIsMac(host) else [])),
              command.Command(MakeCommand(host) + ['all-gdb']),
              command.Command(MAKE_DESTDIR_CMD + [
                  '-C', 'gdb', 'install-strip',
                  ]),
              REMOVE_INFO_DIR,
              ] + InstallDocFiles('gdb', [
                  'COPYING3',
                  command.path.join('gdb', 'NEWS'),
                  ]),
          },
      }

  # TODO(mcgrathr): The ARM cross environment does not supply a termcap
  # library, so it cannot build GDB.
  if host.startswith('arm') and CrossCompiling(host):
    del tools[H('gdb')]

  return tools

def TargetCommands(host, target, command_list):
  # First we have to copy the host tools into a common directory.
  # We can't just have both directories in our PATH, because the
  # compiler looks for the assembler and linker relative to itself.
  commands = PopulateDeps(['%(' + ForHost('binutils_' + target, host) + ')s',
                           '%(' + ForHost('gcc_' + target, host) + ')s'])
  bindir = command.path.join('%(cwd)s', 'all_deps', 'bin')
  commands += [command.Command(cmd, path_dirs=[bindir])
               for cmd in command_list]
  return commands


def TargetLibs(host, target):
  lib_deps = [ForHost(component + '_' + target, host)
              for component in ['binutils', 'gcc']]

  def NewlibFile(subdir, name):
    return command.path.join('%(output)s', target + '-nacl', subdir, name)

  newlib_sysroot = '%(abs_newlib_' + target + ')s'
  newlib_tooldir = '%s/%s-nacl' % (newlib_sysroot, target)

  # See the comment at ConfigureTargetPrep, above.
  newlib_install_data = ' '.join(['STRIPPROG=%(cwd)s/strip_for_target',
                                  '%(abs_newlib)s/install-sh',
                                  '-c', '-s', '-m', '644'])

  iconv_encodings = 'UTF-8,UTF-16LE,UCS-4LE,UTF-16,UCS-4'
  newlib_configure_args = [
      '--disable-libgloss',
      '--enable-newlib-iconv',
      '--enable-newlib-iconv-from-encodings=' + iconv_encodings,
      '--enable-newlib-iconv-to-encodings=' + iconv_encodings,
      '--enable-newlib-io-long-long',
      '--enable-newlib-io-long-double',
      '--enable-newlib-io-c99-formats',
      '--enable-newlib-mb',
      'CFLAGS=-O2',
      'INSTALL_DATA=' + newlib_install_data,
      ]

  newlib_post_install = [
      command.Rename(NewlibFile('lib', 'libc.a'),
                     NewlibFile('lib', 'libcrt_common.a')),
      command.WriteData(NewlibLibcScript(target),
                        NewlibFile('lib', 'libc.a')),
      ] + [
      command.Copy(
          command.path.join('%(pthread_headers)s', header),
          NewlibFile('include', header))
      for header in ('pthread.h', 'semaphore.h')
      ]


  libs = {
      'newlib_' + target: {
          'type': 'build',
          'dependencies': ['newlib'] + lib_deps,
          'inputs': { 'pthread_headers':
                      os.path.join(NACL_DIR, 'src', 'untrusted',
                                   'pthread') },
          'commands': (ConfigureTargetPrep(target) +
                       TargetCommands(host, target, [
                           ConfigureCommand('newlib') +
                           ConfigureHostTool(host) +
                           ConfigureTargetArgs(target) +
                           newlib_configure_args,
                           MakeCommand(host),
                           MAKE_DESTDIR_CMD + ['install-strip'],
                           ]) +
                       newlib_post_install +
                       InstallDocFiles('newlib', ['COPYING.NEWLIB'])),
          },

      'gcc_libs_' + target: {
          'type': 'build',
          'dependencies': (['gcc_libs'] + lib_deps + ['newlib_' + target] +
                           HostGccLibsDeps(host)),
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
              ConfigureGccCommand('gcc_libs', host, target, [
                  '--with-build-sysroot=' + newlib_sysroot,
                  ]),
              GccCommand(host, target,
                         MakeCommand(host) + [
                             'build_tooldir=' + newlib_tooldir,
                             'all-target',
                             ]),
              GccCommand(host, target,
                         MAKE_DESTDIR_CMD + ['install-strip-target']),
              REMOVE_INFO_DIR,
              ],
          },
      }
  return libs

# Compute it once.
NATIVE_TUPLE = pynacl.platform.PlatformTriple()


# For our purposes, "cross-compiling" means not literally that we are
# targetting a host that does not match NATIVE_TUPLE, but that we are
# targetting a host whose binaries we cannot run locally.  So x86-32
# on x86-64 does not count as cross-compiling.  See NATIVE_ENOUGH_MAP, above.
def CrossCompiling(host):
  return (host != NATIVE_TUPLE and
          host not in NATIVE_ENOUGH_MAP.get(NATIVE_TUPLE, {}))


def HostIsWindows(host):
  return host == WINDOWS_HOST_TUPLE


def HostIsMac(host):
  return host == MAC_HOST_TUPLE


# We build target libraries only on Linux for two reasons:
# 1. We only need to build them once.
# 2. Linux is the fastest to build.
# TODO(mcgrathr): In future set up some scheme whereby non-Linux
# bots can build target libraries but not archive them, only verifying
# that the results came out the same as the ones archived by the
# official builder bot.  That will serve as a test of the host tools
# on the other host platforms.
def BuildTargetLibsOn(host):
  return host == LINUX_X86_64_TUPLE


def CollectPackagesForHost(host, targets):
  packages = HostGccLibs(host).copy()
  for target in targets:
    packages.update(HostTools(host, target))
    if BuildTargetLibsOn(host):
      packages.update(TargetLibs(host, target))
  return packages


def CollectPackages(targets):
  packages = CollectSources()

  packages.update(CollectPackagesForHost(NATIVE_TUPLE, targets))

  for host in EXTRA_HOSTS_MAP.get(NATIVE_TUPLE, []):
    packages.update(CollectPackagesForHost(host, targets))

  return packages


PACKAGES = CollectPackages(TARGET_LIST)


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  # TODO(mcgrathr): The bot ought to run some native_client tests
  # using the new toolchain, like the old x86 toolchain bots do.
  tb.Main()

#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import fnmatch
import platform
import re
import sys

import command
import toolchain_main

GIT_REVISIONS = {
    'binutils': '237df3fa4a1d939e6fd1af0c3e5029a25a137310', # tag binutils-2_24
    'gcc': 'ff9fa53fcbbfbef66227f0a60704b1f5c2951b97',
    'newlib': '9f95ad0b4875d153b9d138090e61ac4c24e9a87d',
    }

GIT_BASE_URL = 'https://chromium.googlesource.com/native_client'


def CollectSources():
  sources = {}

  for package in GIT_REVISIONS:
    sources[package] = {
        'type': 'source',
        'commands': [
            command.SyncGitRepo('%s/nacl-%s.git' % (GIT_BASE_URL, package),
                                '%(output)s', GIT_REVISIONS[package])
            ]
        }

  # The gcc_libs component gets the whole GCC source tree.
  sources['gcc_libs'] = sources['gcc']

  # The gcc component omits all the source directories that are used solely
  # for building target libraries.  We don't want those included in the
  # input hash calculation so that we don't rebuild the compiler when the
  # the only things that have changed are target libraries.
  sources['gcc'] = {
        'type': 'build',
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


# Map of native host tuple to extra tuples that it cross-builds for.
EXTRA_HOSTS_MAP = {
# TODO(mcgrathr): Cross-building binutils is waiting for an upstream
# makefile fix, see https://sourceware.org/ml/binutils/2013-12/msg00107.html
#    'i686-linux': ['arm-linux-gnueabihf'],
    }

# The list of targets to build toolchains for.
TARGET_LIST = ['arm']

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

  native, extra_cc_args = NativeTuple()
  is_cross = host != native

  if is_cross:
    extra_cc_args = []
    configure_args.append('--host=' + host)
  elif extra_cc_args:
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
    return MAKE_PARALLEL_CMD + ['HAVE_LIBICONV=no']
  return MAKE_PARALLEL_CMD


# Return the 'make check' command to run.
# When cross-compiling, don't try to run test suites.
def MakeCheckCommand(host):
  native, _ = NativeTuple()
  if host != native:
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
  return [
      command.RemoveDirectory('build'),
      command.Mkdir('build'),
      ] + [command.Command(cmd, cwd='build')
           for cmd in command_lines]


def UnpackSrc(is_gzip):
  if is_gzip:
    extract = EXTRACT_STRIP_TGZ
  else:
    extract = EXTRACT_STRIP_TBZ2
  return [
      command.RemoveDirectory('src'),
      command.Mkdir('src'),
      command.Command(extract + ['%(src)s'], cwd='src'),
      ]


def PopulateDeps(dep_dirs):
  commands = [command.RemoveDirectory('all_deps'),
              command.Mkdir('all_deps')]
  commands += [command.Command('cp -r "%s/"* all_deps' % dirname, shell=True)
               for dirname in dep_dirs]
  return commands


def WithDepsOptions(options):
  return ['--with-' + option + '=%(abs_all_deps)s' for option in options]


# Return the component name we'll use for a base component name and
# a host tuple.  The component names cannot contain dashes or other
# non-identifier characters, because the names of the files uploaded
# to Google Storage are constrained.  GNU configuration tuples contain
# dashes, which we translate to underscores.
def ForHost(component_name, host):
  return component_name + '_' + re.sub(r'[^A-Za-z0-9_/.]', '_', host)


# These are libraries that go into building the compiler itself.
# TODO(mcgrathr): Make these use source targets in some fashion.
def HostGccLibs(host):
  def H(component_name):
    return ForHost(component_name, host)
  host_gcc_libs = {
      H('gmp'): {
          'type': 'build',
          'tar_src': 'third_party/gmp/gmp-5.1.3.tar.bz2',
          'unpack_commands': UnpackSrc(False),
          'hashed_inputs': {'src': 'src'},
          'commands': CommandsInBuild([
              CONFIGURE_CMD + ConfigureHostLib(host) + [
                  '--with-sysroot=%(abs_output)s',
                  '--enable-cxx',
                  # Without this, the built library will assume the
                  # instruction set details available on the build machine.
                  # With this, it dynamically chooses what code to use based
                  # on the details of the actual host CPU at runtime.
                  '--enable-fat',
                  ],
              MakeCommand(host),
              MakeCheckCommand(host),
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]),
          },
      H('mpfr'): {
          'type': 'build',
          'dependencies': [H('gmp')],
          'tar_src': 'third_party/mpfr/mpfr-3.1.2.tar.bz2',
          'unpack_commands': (UnpackSrc(False) +
                              PopulateDeps(['%(' + H('gmp') + ')s'])),
          'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
          'commands': CommandsInBuild([
              CONFIGURE_CMD + ConfigureHostLib(host) + WithDepsOptions(
                  ['sysroot', 'gmp']),
              MakeCommand(host),
              MakeCheckCommand(host),
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]),
          },
      H('mpc'): {
          'type': 'build',
          'dependencies': [H('gmp'), H('mpfr')],
          'tar_src': 'third_party/mpc/mpc-1.0.1.tar.gz',
          'unpack_commands': (UnpackSrc(True) +
                              PopulateDeps(['%(' + H('gmp') + ')s',
                                            '%(' + H('mpfr') + ')s'])),
          'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
          'commands': CommandsInBuild([
              CONFIGURE_CMD + ConfigureHostLib(host) + WithDepsOptions(
                  ['sysroot', 'gmp', 'mpfr']),
              MakeCommand(host),
              MakeCheckCommand(host),
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]),
          },
      H('isl'): {
          'type': 'build',
          'dependencies': [H('gmp')],
          'tar_src': 'third_party/cloog/isl-0.12.1.tar.bz2',
          'unpack_commands': (UnpackSrc(False) +
                              PopulateDeps(['%(' + H('gmp') + ')s'])),
          'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
          'commands': CommandsInBuild([
              CONFIGURE_CMD + ConfigureHostLib(host) + WithDepsOptions([
                  'sysroot',
                  'gmp-prefix',
                  ]),
              MakeCommand(host),
              MakeCheckCommand(host),
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]) + [
                  # The .pc files wind up containing some absolute paths
                  # that make the output depend on the build directory name.
                  # The dependents' configure scripts don't need them anyway.
                  command.RemoveDirectory(command.path.join(
                      '%(output)s', 'lib', 'pkgconfig')),
                  ],
          },
      H('cloog'): {
          'type': 'build',
          'dependencies': [H('gmp'), H('isl')],
          'tar_src': 'third_party/cloog/cloog-0.18.1.tar.gz',
          'unpack_commands': (UnpackSrc(True) +
                              PopulateDeps(['%(' + H('gmp') + ')s',
                                            '%(' + H('isl') + ')s'])),
          'hashed_inputs': {'src': 'src', 'all_deps': 'all_deps'},
          'commands': CommandsInBuild([
              CONFIGURE_CMD + ConfigureHostLib(host) + [
                  '--with-bits=gmp',
                  '--with-isl=system',
                  ] + WithDepsOptions(['sysroot',
                                       'gmp-prefix',
                                       'isl-prefix']),
              MakeCommand(host),
              MakeCheckCommand(host),
              MAKE_DESTDIR_CMD + ['install-strip'],
              ]) + [
                  # The .pc files wind up containing some absolute paths
                  # that make the output depend on the build directory name.
                  # The dependents' configure scripts don't need them anyway.
                  command.RemoveDirectory(command.path.join(
                      '%(output)s', 'lib', 'pkgconfig')),
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
  native, _ = NativeTuple()
  components = ['binutils_' + target]
  if host != native:
    components.append('gcc_' + target)
    host = native
  return [ForHost(component, host) for component in components]


def GccCommand(host, target, cmd):
  components_for_path = GccDeps(host, target)
  return command.Command(
      cmd, path_dirs=[command.path.join('%(abs_' + component + ')s', 'bin')
                      for component in components_for_path])


def ConfigureGccCommand(source_component, host, target, extra_args=[]):
  target_cflagstr = ' '.join(CommonTargetCflags(target))
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
          'CFLAGS_FOR_TARGET=' + target_cflagstr,
          'CXXFLAGS_FOR_TARGET=' + target_cflagstr,
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
                         MakeCommand(['all-gcc', 'LIMITS_H_TEST=true'])),
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
      }
  return tools


# configure defaults to -g -O2 but passing an explicit option overrides that.
# So we have to list -g -O2 explicitly since we need to add -mtp=soft.
def CommonTargetCflags(target):
  if target == 'arm':
    tls_option = '-mtp=soft'
  else:
    tls_option = '-mtls-use-call'
  return ['-g', '-O2', tls_option]


def NewlibTargetCflags(target):
  options = CommonTargetCflags(target) + [
      '-D_I386MACH_ALLOW_HW_INTERRUPTS',
      ]
  return ' '.join(options)


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
      'CFLAGS_FOR_TARGET=' + NewlibTargetCflags(target),
      'INSTALL_DATA=' + newlib_install_data,
      ]

  newlib_post_install = [
      command.Remove(NewlibFile('include', 'pthread.h')),
      command.Rename(NewlibFile('lib', 'libc.a'),
                     NewlibFile('lib', 'libcrt_common.a')),
      command.WriteData(NewlibLibcScript(target),
                        NewlibFile('lib', 'libc.a')),
      ]

  libs = {
      'newlib_' + target: {
          'type': 'build',
          'dependencies': ['newlib'] + lib_deps,
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


def NativeTuple():
  if sys.platform.startswith('linux'):
    machine = platform.machine().lower()
    if machine.startswith('arm'):
      return ('arm-linux-gnueabihf', [])
    if any(fnmatch.fnmatch(machine, pattern) for pattern in
           ['x86_64*', 'amd64*', 'x64*', 'i?86*']):
      # We build the tools for x86-32 hosts so they will run on either x86-32
      # or x86-64 hosts (with the right compatibility libraries installed).
      # So for an x86-64 host, we call x86-32 the "native" machine.
      return ('i686-linux', ['-m32'])
    raise Exception('Machine %s not recognized' % machine)
  elif sys.platform.startswith('win'):
    return ('i686-w64-mingw32', [])
  elif sys.platform.startswith('darwin'):
    return ('x86_64-apple-darwin', [])
  raise Exception('Platform %s not recognized' % sys.platform)


def HostIsWindows(host):
  return host == 'i686-w64-mingw32'


# We build target libraries only on Linux for two reasons:
# 1. We only need to build them once.
# 2. Linux is the fastest to build.
# TODO(mcgrathr): In future set up some scheme whereby non-Linux
# bots can build target libraries but not archive them, only verifying
# that the results came out the same as the ones archived by the
# official builder bot.  That will serve as a test of the host tools
# on the other host platforms.
def BuildTargetLibsOn(host):
  return host == 'i686-linux'


def CollectPackagesForHost(host, targets):
  packages = HostGccLibs(host).copy()
  for target in targets:
    packages.update(HostTools(host, target))
    if BuildTargetLibsOn(host):
      packages.update(TargetLibs(host, target))
  return packages


def CollectPackages(targets):
  packages = CollectSources()

  native, _ = NativeTuple()
  packages.update(CollectPackagesForHost(native, targets))

  for host in EXTRA_HOSTS_MAP.get(native, []):
    packages.update(CollectPackagesForHost(host, targets))

  return packages


PACKAGES = CollectPackages(TARGET_LIST)


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  # TODO(mcgrathr): The bot ought to run some native_client tests
  # using the new toolchain, like the old x86 toolchain bots do.
  tb.Main()

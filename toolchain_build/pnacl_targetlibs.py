#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl target libs."""

import fnmatch
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.gsd_storage
import pynacl.platform

import command
import pnacl_commands


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)


# Return the path to the local copy of the driver script.
# msys should be false if the path will be called directly rather than passed to
# an msys or cygwin tool such as sh or make.
def PnaclTool(toolname, msys=True):
  if not msys and pynacl.platform.IsWindows():
    ext = '.bat'
  else:
    ext = ''

  return command.path.join('%(cwd)s', 'driver', 'pnacl-' + toolname + ext)

# PNaCl tools for newlib's environment, e.g. CC_FOR_TARGET=/path/to/pnacl-clang
TOOL_ENV_NAMES = { 'CC': 'clang', 'CXX': 'clang++', 'AR': 'ar', 'NM': 'nm',
                   'RANLIB': 'ranlib', 'READELF': 'readelf',
                   'OBJDUMP': 'illegal', 'AS': 'illegal', 'LD': 'illegal',
                   'STRIP': 'illegal' }
TARGET_TOOLS = [ tool + '_FOR_TARGET=' + PnaclTool(name)
                 for tool, name in TOOL_ENV_NAMES.iteritems() ]

def MakeCommand():
  make_command = ['make']
  if (not pynacl.platform.IsWindows() and
      not command.Runnable.use_cygwin):
    # The make that ships with msys sometimes hangs when run with -j.
    # The ming32-make that comes with the compiler itself reportedly doesn't
    # have this problem, but it has issues with pathnames with LLVM's build.
    make_command.append('-j%(cores)s')
  return make_command

# Return the component name to use for a component name with
# a host triple. GNU configuration triples contain dashes, which are converted
# to underscores so the names are legal for Google Storage.
def Mangle(component_name, extra):
  return component_name + '_' + pynacl.gsd_storage.LegalizeName(extra)

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
                       host_64bit=fnmatch.fnmatch(host, '*x86_64*'),
                       extra_config=['BPREFIXES=%(abs_' + Mangle('llvm', host) +
                        ')s %(abs_' + Mangle('binutils_pnacl', host) + ')s'])
      ]


def BiasedBitcodeTargetFlag(arch):
  flagmap = {
      # Arch     Target                           Extra flags.
      'x86-64': ('x86_64-unknown-nacl',           []),
      'x86-32': ('i686-unknown-nacl',             []),
      'arm':    ('armv7-unknown-nacl-gnueabihf',  ['-mfloat-abi=hard']),
      # MIPS doesn't use biased bitcode:
      'mips32': ('le32-unknown-nacl',             []),
  }
  return ['--target=%s' % flagmap[arch][0]] + flagmap[arch][1]


def TargetBCLibCflags(bias_arch):
  flags = '-g -O2 -mllvm -inline-threshold=5'
  if bias_arch != 'portable':
    flags += ' ' + ' '.join(BiasedBitcodeTargetFlag(bias_arch))
  return flags

def NewlibIsystemCflags(bias_arch):
  return ' '.join([
    '-isystem',
    command.path.join('%(' + Mangle('abs_newlib', bias_arch) +')s', 'include')])

def LibCxxCflags(bias_arch):
  # HAS_THREAD_LOCAL is used by libc++abi's exception storage, the fallback is
  # pthread otherwise.
  return ' '.join([TargetBCLibCflags(bias_arch), NewlibIsystemCflags(bias_arch),
                   '-DHAS_THREAD_LOCAL=1'])


def LibStdcxxCflags(bias_arch):
  return ' '.join([TargetBCLibCflags(bias_arch),
                   NewlibIsystemCflags(bias_arch)])


# Build a single object file as bitcode.
def BuildTargetBitcodeCmd(source, output, bias_arch):
  flags = ['-Wall', '-Werror', '-O2', '-c']
  if bias_arch != 'portable':
    [flags.append(flag) for flag in BiasedBitcodeTargetFlag(bias_arch)]
  sysinclude = Mangle('newlib', bias_arch)
  return command.Command(
    [PnaclTool('clang', msys=False)] + flags + [
        '-isystem', '%(' + sysinclude + ')s/include',
     command.path.join('%(src)s', source),
     '-o', command.path.join('%(output)s', output)])


# Build a single object file as native code.
def BuildTargetNativeCmd(sourcefile, output, arch, extra_flags=[],
                         source_dir='%(src)s', output_dir='%(cwd)s'):
  return command.Command(
    [PnaclTool('clang', msys=False),
     '--pnacl-allow-native', '--pnacl-allow-translate', '-Wall', '-Werror',
     '-arch', arch, '--pnacl-bias=' + arch, '-O3',
     # TODO(dschuff): this include breaks the input encapsulation for build
     # rules.
     '-I%(top_srcdir)s/..', '-isystem', '%(newlib_portable)s/include', '-c'] +
    extra_flags +
    [command.path.join(source_dir, sourcefile),
     '-o', command.path.join(output_dir, output)])


def BuildLibgccEhCmd(sourcefile, output, arch):
  # Return a command to compile a file from libgcc_eh (see comments in at the
  # rule definition below).
  flags_common = ['-DENABLE_RUNTIME_CHECKING', '-g', '-O2', '-W', '-Wall',
                  '-Wwrite-strings', '-Wcast-qual', '-Wstrict-prototypes',
                  '-Wmissing-prototypes', '-Wold-style-definition',
                  '-DIN_GCC', '-DCROSS_DIRECTORY_STRUCTURE', '-DIN_LIBGCC2',
                  '-D__GCC_FLOAT_NOT_NEEDED', '-Dinhibit_libc',
                  '-DHAVE_CC_TLS', '-DHIDE_EXPORTS',
                  '-fno-stack-protector', '-fexceptions',
                  '-fvisibility=hidden',
                  '-I.', '-I../.././gcc', '-I%(abs_gcc_src)s/gcc/libgcc',
                  '-I%(abs_gcc_src)s/gcc', '-I%(abs_gcc_src)s/include',
                  '-isystem', './include']
  # For x86 we use nacl-gcc to build libgcc_eh because of some issues with
  # LLVM's handling of the gcc intrinsics used in the library. See
  # https://code.google.com/p/nativeclient/issues/detail?id=1933
  # and http://llvm.org/bugs/show_bug.cgi?id=8541
  # For ARM, LLVM does work and we use it to avoid dealing with the fact that
  # arm-nacl-gcc uses different libgcc support functions than PNaCl.
  if arch in ('arm', 'mips32'):
    cc = PnaclTool('clang', msys=False)
    flags_naclcc = ['-arch', arch, '--pnacl-bias=' + arch,
                    '--pnacl-allow-translate', '--pnacl-allow-native']
  else:
    os_name = pynacl.platform.GetOS()
    arch_name = pynacl.platform.GetArch()
    platform_dir = '%s_%s' % (os_name, arch_name)
    newlib_dir = 'nacl_x86_newlib'

    nnacl_dir = os.path.join(NACL_DIR, 'toolchain', platform_dir,
                             newlib_dir, 'bin')
    gcc_binaries = {
        'x86-32': 'i686-nacl-gcc',
        'x86-64': 'x86_64-nacl-gcc',
    }

    cc = os.path.join(nnacl_dir, gcc_binaries[arch])
    flags_naclcc = []
  return command.Command([cc] + flags_naclcc + flags_common +
                         ['-c',
                          command.path.join('%(gcc_src)s', 'gcc', sourcefile),
                          '-o', output])



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


def BitcodeLibs(host, bias_arch):
  def H(component_name):
    return Mangle(component_name, host)
  def B(component_name):
    return Mangle(component_name, bias_arch)
  def BcSubdir(subdir, bias_arch):
    if bias_arch == 'portable':
      return subdir
    else:
      return subdir + '-bc-' + bias_arch
  libs = {
      B('newlib'): {
          'type': 'build',
          'output_subdir': BcSubdir('usr', bias_arch),
          'dependencies': [ 'newlib_src', H('llvm'), H('binutils_pnacl')],
          'inputs': { 'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands' :
              CopyDriverForTargetLib(host) + [
              command.SkipForIncrementalCommand(
                  ['sh', '%(newlib_src)s/configure'] +
                  TARGET_TOOLS +
                  ['CFLAGS_FOR_TARGET=' + TargetBCLibCflags(bias_arch) +
                   ' -allow-asm',
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
      B('libcxx'): {
          'type': 'build',
          'output_subdir': BcSubdir('usr', bias_arch),
          'dependencies': ['libcxx_src', 'libcxxabi_src', 'llvm_src', 'gcc_src',
                           H('llvm'), H('binutils_pnacl'), B('newlib')],
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
                   '-DCMAKE_C_FLAGS=-std=gnu11 ' + LibCxxCflags(bias_arch),
                   '-DCMAKE_CXX_FLAGS=-std=gnu++11 ' + LibCxxCflags(bias_arch),
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
      B('libstdcxx'): {
          'type': 'build',
          'output_subdir': BcSubdir('usr', bias_arch),
          'dependencies': ['gcc_src', 'gcc_src', H('llvm'), H('binutils_pnacl'),
                           B('newlib')],
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
                  'CFLAGS=' + LibStdcxxCflags(bias_arch),
                  'CXXFLAGS=' + LibStdcxxCflags(bias_arch),
                  'CPPFLAGS=' + NewlibIsystemCflags(bias_arch),
                  'CFLAGS_FOR_TARGET=' + LibStdcxxCflags(bias_arch),
                  'CXXFLAGS_FOR_TARGET=' + LibStdcxxCflags(bias_arch),
                  '--host=armv7-none-linux-gnueabi',
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
      B('libs_support_bitcode'): {
          'type': 'build',
          'output_subdir': BcSubdir('lib', bias_arch),
          'dependencies': [ B('newlib'), H('llvm'), H('binutils_pnacl')],
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
              BuildTargetBitcodeCmd('crti.c', 'crti.bc', bias_arch),
              # Install crtbegin bitcode (__cxa_finalize for C++)
              BuildTargetBitcodeCmd('crtbegin.c', 'crtbegin.bc', bias_arch),
              # Stubs for _Unwind_* functions when libgcc_eh is not included in
              # the native link).
              BuildTargetBitcodeCmd('unwind_stubs.c', 'unwind_stubs.bc',
                                    bias_arch),
              BuildTargetBitcodeCmd('sjlj_eh_redirect.c',
                                    'sjlj_eh_redirect.bc', bias_arch),
              # libpnaclmm.a (__atomic_* library functions).
              BuildTargetBitcodeCmd('pnaclmm.c', 'pnaclmm.bc', bias_arch),
              command.Command([
                  PnaclTool('ar'), 'rc',
                  command.path.join('%(output)s', 'libpnaclmm.a'),
                  command.path.join('%(output)s', 'pnaclmm.bc')]),
          ],
      },
  }
  return libs


def AeabiReadTpCmd(arch):
  if arch == 'arm':
    return [BuildTargetNativeCmd('aeabi_read_tp.S', 'aeabi_read_tp.o', arch)]
  else:
    return []


def NativeLibs(host, arch):
  def H(component_name):
    return Mangle(component_name, host)
  setjmp_arch = arch
  if setjmp_arch == 'x86-32-nonsfi':
    setjmp_arch = 'x86-32'
  libs = {
      Mangle('libs_support_native', arch): {
          'type': 'build',
          'output_subdir': 'lib-' + arch,
          'dependencies': [ 'newlib_src', 'newlib_portable',
                            H('llvm'), H('binutils_pnacl')],
          # These libs include
          # arbitrary stuff from native_client/src/{include,untrusted,trusted}
          'inputs': { 'src': os.path.join(NACL_DIR, 'pnacl', 'support'),
                      'include': os.path.join(NACL_DIR, 'src'),
                      'newlib_subset': os.path.join(
                          NACL_DIR, 'src', 'third_party_mod',
                          'pnacl_native_newlib_subset'),
                      'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands':
              CopyDriverForTargetLib(host) + [
              BuildTargetNativeCmd('crtbegin.c', 'crtbegin.o', arch,
                                   output_dir='%(output)s'),
              BuildTargetNativeCmd('crtbegin.c', 'crtbegin_for_eh.o', arch,
                                   ['-DLINKING_WITH_LIBGCC_EH'],
                                   output_dir='%(output)s'),
              BuildTargetNativeCmd('crtend.c', 'crtend.o', arch,
                                   output_dir='%(output)s'),
              # libcrt_platform.a
              BuildTargetNativeCmd('pnacl_irt.c', 'pnacl_irt.o', arch),
              BuildTargetNativeCmd(
                  'setjmp_%s.S' % setjmp_arch.replace('-', '_'),
                  'setjmp.o', arch),
              BuildTargetNativeCmd('string.c', 'string.o', arch,
                                   ['-std=c99'],
                                   source_dir='%(newlib_subset)s'),
              # Pull in non-errno __ieee754_fmod from newlib and rename it to
              # fmod. This is to support the LLVM frem instruction.
              BuildTargetNativeCmd(
                  'e_fmod.c', 'e_fmod.o', arch,
                  ['-std=c99', '-I%(abs_newlib_src)s/newlib/libm/common/',
                   '-D__ieee754_fmod=fmod'],
                  source_dir='%(abs_newlib_src)s/newlib/libm/math'),
              BuildTargetNativeCmd(
                  'ef_fmod.c', 'ef_fmod.o', arch,
                  ['-std=c99', '-I%(abs_newlib_src)s/newlib/libm/common/',
                   '-D__ieee754_fmodf=fmodf'],
                  source_dir='%(abs_newlib_src)s/newlib/libm/math')] +
              AeabiReadTpCmd(arch) + [
              command.Command(' '.join([
                  PnaclTool('ar'), 'rc',
                  command.path.join('%(output)s', 'libcrt_platform.a'),
                  '*.o']), shell=True),
              # Dummy IRT shim
              BuildTargetNativeCmd('dummy_shim_entry.c', 'dummy_shim_entry.o',
                                   arch),
              command.Command([PnaclTool('ar'), 'rc',
                               command.path.join('%(output)s',
                                                 'libpnacl_irt_shim_dummy.a'),
                               'dummy_shim_entry.o']),
          ],
      },
      Mangle('compiler_rt', arch): {
          'type': 'build',
          'output_subdir': 'lib-' + arch,
          'dependencies': [ 'compiler_rt_src', H('llvm'), H('binutils_pnacl')],
          'inputs': { 'driver': os.path.join(NACL_DIR, 'pnacl', 'driver')},
          'commands':
              CopyDriverForTargetLib(host) + [
              command.Command(MakeCommand() + [
                  '-f',
                  command.path.join('%(compiler_rt_src)s', 'lib',
                                    'Makefile-pnacl'),
                  'libgcc.a', 'CC=' + PnaclTool('clang'),
                  'AR=' + PnaclTool('ar')] +
                  ['SRC_DIR=' + command.path.join('%(abs_compiler_rt_src)s',
                                                  'lib'),
                   'CFLAGS=-arch ' + arch + ' -DPNACL_' +
                    arch.replace('-', '_') + ' --pnacl-allow-translate -O3']),
              command.Copy('libgcc.a', os.path.join('%(output)s', 'libgcc.a')),
          ],
      },
  }
  if arch != 'x86-32-nonsfi':
    libs.update({
      Mangle('libgcc_eh', arch): {
          'type': 'build',
          'output_subdir': 'lib-' + arch,
          'dependencies': [ 'gcc_src', H('llvm'), H('binutils_pnacl')],
          'inputs': { 'scripts': os.path.join(NACL_DIR, 'pnacl', 'scripts'),
                      'driver': os.path.join(NACL_DIR, 'pnacl', 'driver') },
          'commands':
              # Instead of trying to use gcc's build system to build only
              # libgcc_eh, we just build the C files and archive them manually.
              CopyDriverForTargetLib(host) + [
              command.RemoveDirectory('include'),
              command.Mkdir('include'),
              command.Copy(os.path.join('%(gcc_src)s', 'gcc',
                           'unwind-generic.h'),
                           os.path.join('include', 'unwind.h')),
              command.Copy(os.path.join('%(scripts)s', 'libgcc-tconfig.h'),
                           'tconfig.h'),
              command.WriteData('', 'tm.h'),
              BuildLibgccEhCmd('unwind-dw2.c', 'unwind-dw2.o', arch),
              BuildLibgccEhCmd('unwind-dw2-fde-glibc.c',
                               'unwind-dw2-fde-glibc.o', arch),
              command.Command([PnaclTool('ar'), 'rc',
                               command.path.join('%(output)s', 'libgcc_eh.a'),
                               'unwind-dw2.o', 'unwind-dw2-fde-glibc.o']),
          ],
      },
    })
  return libs

def UnsandboxedIRT(arch):
  libs = {
      Mangle('unsandboxed_irt', arch): {
          'type': 'build',
          'output_subdir': 'lib-' + arch,
          'inputs': { 'support': os.path.join(NACL_DIR, 'pnacl', 'support') },
          'commands': [
              # TODO(dschuff): this include path breaks the input encapsulation
              # for build rules.
              command.Command([
                  'gcc', '-m32', '-O2', '-Wall', '-Werror',
                  '-I%(top_srcdir)s/..', '-DNACL_LINUX=1',
                  '-c', command.path.join('%(support)s', 'unsandboxed_irt.c'),
                  '-o', command.path.join('%(output)s', 'unsandboxed_irt.o')]),
          ],
      },
  }
  return libs

#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl target libs."""

import fnmatch
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.gsd_storage
import pynacl.platform

import command
import pnacl_commands


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

CLANG_VER = '3.4'

# Return the path to the local copy of the driver script.
# msys should be false if the path will be called directly rather than passed to
# an msys or cygwin tool such as sh or make.
def PnaclTool(toolname, msys=True):
  if not msys and pynacl.platform.IsWindows():
    ext = '.bat'
  else:
    ext = ''
  return command.path.join('%(abs_target_lib_compiler)s',
                           'bin', 'pnacl-' + toolname + ext)

# PNaCl tools for newlib's environment, e.g. CC_FOR_TARGET=/path/to/pnacl-clang
TOOL_ENV_NAMES = { 'CC': 'clang', 'CXX': 'clang++', 'AR': 'ar', 'NM': 'nm',
                   'RANLIB': 'ranlib', 'READELF': 'readelf',
                   'OBJDUMP': 'illegal', 'AS': 'illegal', 'LD': 'illegal',
                   'STRIP': 'illegal' }
TARGET_TOOLS = [ tool + '_FOR_TARGET=' + PnaclTool(name)
                 for tool, name in TOOL_ENV_NAMES.iteritems() ]

def MakeCommand():
  make_command = ['make']
  if not pynacl.platform.IsWindows():
    # The make that ships with msys sometimes hangs when run with -j.
    # The ming32-make that comes with the compiler itself reportedly doesn't
    # have this problem, but it has issues with pathnames with LLVM's build.
    make_command.append('-j%(cores)s')
  return make_command

# Return the component name to use for a component name with
# a host triple. GNU configuration triples contain dashes, which are converted
# to underscores so the names are legal for Google Storage.
def GSDJoin(*args):
  return '_'.join([pynacl.gsd_storage.LegalizeName(arg) for arg in args])


# Copy the compiled bitcode archives used for linking C programs into the the
# current working directory. This allows the driver in the working directory to
# be used in cases which need the ability to link pexes (e.g. CMake
# try-compiles, LLVM testsuite, or libc++ testsuite). For now this also requires
# a build of libnacl however, which is driven by the buildbot script or
# external test script. TODO(dschuff): add support to drive the LLVM and libcxx
# test suites from toolchain_build rules.
def CopyBitcodeCLibs(bias_arch):
  return [
      command.RemoveDirectory('usr'),
      command.Mkdir('usr'),
      command.Command('cp -r %(' +
                      GSDJoin('abs_libs_support_bitcode', bias_arch) +
                      ')s usr', shell=True),
      command.Command('cp -r %(' + GSDJoin('abs_newlib', bias_arch) +
                      ')s/* usr', shell=True),
      ]


def BiasedBitcodeTriple(bias_arch):
  return 'le32-nacl' if bias_arch == 'le32' else bias_arch + '_bc-nacl'

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
  if bias_arch != 'le32':
    flags += ' ' + ' '.join(BiasedBitcodeTargetFlag(bias_arch))
  return flags

def NewlibIsystemCflags(bias_arch):
  return ' '.join([
    '-isystem',
    command.path.join('%(' + GSDJoin('abs_newlib', bias_arch) +')s',
                      BiasedBitcodeTriple(bias_arch), 'include')])

def LibCxxCflags(bias_arch):
  # HAS_THREAD_LOCAL is used by libc++abi's exception storage, the fallback is
  # pthread otherwise.
  return ' '.join([TargetBCLibCflags(bias_arch), NewlibIsystemCflags(bias_arch),
                   '-DHAS_THREAD_LOCAL=1'])


def LibStdcxxCflags(bias_arch):
  return ' '.join([TargetBCLibCflags(bias_arch),
                   NewlibIsystemCflags(bias_arch)])


# Build a single object file as bitcode.
def BuildTargetBitcodeCmd(source, output, bias_arch, output_dir='%(cwd)s'):
  flags = ['-Wall', '-Werror', '-O2', '-c']
  if bias_arch != 'le32':
    flags.extend(BiasedBitcodeTargetFlag(bias_arch))
  flags.extend(NewlibIsystemCflags(bias_arch).split())
  return command.Command(
      [PnaclTool('clang', msys=False)] + flags + [
          command.path.join('%(src)s', source),
     '-o', command.path.join(output_dir, output)])


# Build a single object file as native code.
def BuildTargetNativeCmd(sourcefile, output, arch, extra_flags=[],
                         source_dir='%(src)s', output_dir='%(cwd)s'):
  return command.Command(
    [PnaclTool('clang', msys=False),
     '--pnacl-allow-native', '--pnacl-allow-translate', '-Wall', '-Werror',
     '-arch', arch, '--pnacl-bias=' + arch, '-O3',
     # TODO(dschuff): this include breaks the input encapsulation for build
     # rules.
     '-I%(top_srcdir)s/..','-c'] +
    NewlibIsystemCflags('le32').split() +
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



def TargetLibsSrc(GitSyncCmds):
  newlib_sys_nacl = command.path.join('%(output)s',
                                      'newlib', 'libc', 'sys', 'nacl')
  source = {
      'newlib_src': {
          'type': 'source',
          'output_dirname': 'pnacl-newlib',
          'commands': [
              # Clean any headers exported from the NaCl tree before syncing.
              command.CleanGitWorkingDir(
                  '%(output)s', os.path.join('newlib', 'libc', 'include'))] +
              GitSyncCmds('nacl-newlib') +
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
          'commands': GitSyncCmds('compiler-rt'),
      },
      'gcc_src': {
          'type': 'source',
          'output_dirname': 'pnacl-gcc',
          'commands': GitSyncCmds('gcc'),
      },
  }
  return source


def BitcodeLibs(bias_arch, is_canonical):
  def B(component_name):
    return GSDJoin(component_name, bias_arch)
  bc_triple = BiasedBitcodeTriple(bias_arch)
  clang_libdir = os.path.join(
      '%(output)s', 'lib', 'clang', CLANG_VER, 'lib', bc_triple)
  libc_libdir = os.path.join('%(output)s', bc_triple, 'lib')
  libs = {
      B('newlib'): {
          'type': 'build' if is_canonical else 'work',
          'dependencies': [ 'newlib_src', 'target_lib_compiler'],
          'commands' : [
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
              # We configured newlib with target=le32-nacl to get its pure C
              # implementation, so rename its output dir (which matches the
              # target to the output dir for the package we are building)
              command.Rename(os.path.join('%(output)s', 'le32-nacl'),
                             os.path.join('%(output)s', bc_triple)),
              # Copy nacl_random.h, used by libc++. It uses the IRT, so should
              # be safe to include in the toolchain.
              command.Mkdir(
                  os.path.join('%(output)s', bc_triple, 'include', 'nacl')),
              command.Copy(os.path.join('%(top_srcdir)s', 'src', 'untrusted',
                                        'nacl', 'nacl_random.h'),
                           os.path.join(
                               '%(output)s', bc_triple, 'include', 'nacl',
                               'nacl_random.h')),
              # Remove the 'share' directory from the biased builds; the data is
              # duplicated exactly and takes up 2MB per package.
              command.RemoveDirectory(os.path.join('%(output)s', 'share'),
                             run_cond = lambda x: bias_arch != 'le32'),
          ],
      },
      B('libcxx'): {
          'type': 'build' if is_canonical else 'work',
          'dependencies': ['libcxx_src', 'libcxxabi_src', 'llvm_src', 'gcc_src',
                           'target_lib_compiler', B('newlib'),
                           B('libs_support_bitcode')],
          'commands' :
              CopyBitcodeCLibs(bias_arch) + [
              command.SkipForIncrementalCommand(
                  ['cmake', '-G', 'Unix Makefiles',
                   '-DCMAKE_C_COMPILER_WORKS=1',
                   '-DCMAKE_CXX_COMPILER_WORKS=1',
                   '-DCMAKE_INSTALL_PREFIX=',
                   '-DCMAKE_BUILD_TYPE=Release',
                   '-DCMAKE_C_COMPILER=' + PnaclTool('clang'),
                   '-DCMAKE_CXX_COMPILER=' + PnaclTool('clang++'),
                   '-DCMAKE_SYSTEM_NAME=nacl',
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
                   # The lit flags are used by the libcxx testsuite, which is
                   # currenty driven by an external script.
                   '-DLLVM_LIT_ARGS=--verbose  --param shell_prefix="' +
                    os.path.join(NACL_DIR,'run.py') +' -arch env --retries=1" '+
                    '--param exe_suffix=".pexe" --param use_system_lib=true ' +
                    '--param cxx_under_test="' +  os.path.join(NACL_DIR,
                        'toolchain/linux_x86/pnacl_newlib/bin/pnacl-clang++') +
                    '" '+
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
              command.Command([
                  'make',
                  'DESTDIR=' + os.path.join('%(abs_output)s', bc_triple),
                  'VERBOSE=1',
                  'install']),
          ],
      },
      B('libstdcxx'): {
          'type': 'build' if is_canonical else 'work',
          'dependencies': ['gcc_src', 'gcc_src', 'target_lib_compiler',
                           B('newlib')],
          'commands' : [
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
              command.Command([
                  'make',
                  'DESTDIR=' + os.path.join('%(abs_output)s', bc_triple),
                  'install-data']),
              command.RemoveDirectory(
                  os.path.join('%(output)s', bc_triple, 'share')),
              command.Remove(os.path.join('%(output)s', bc_triple, 'lib',
                                          'libstdc++*-gdb.py')),
              command.Copy(
                  os.path.join('src', '.libs', 'libstdc++.a'),
                  os.path.join('%(output)s', bc_triple, 'lib', 'libstdc++.a')),
          ],
      },
      B('libs_support_bitcode'): {
          'type': 'build' if is_canonical else 'work',
          'dependencies': [ B('newlib'), 'target_lib_compiler'],
          'inputs': { 'src': os.path.join(NACL_DIR,
                                          'pnacl', 'support', 'bitcode')},
          'commands': [
              command.Mkdir(clang_libdir, parents=True),
              command.Mkdir(libc_libdir, parents=True),
              # Two versions of crt1.x exist, for different scenarios (with and
              # without EH).  See:
              # https://code.google.com/p/nativeclient/issues/detail?id=3069
              command.Copy(command.path.join('%(src)s', 'crt1.x'),
                           command.path.join(libc_libdir, 'crt1.x')),
              command.Copy(command.path.join('%(src)s', 'crt1_for_eh.x'),
                           command.path.join(libc_libdir, 'crt1_for_eh.x')),
              # Install crti.bc (empty _init/_fini)
              BuildTargetBitcodeCmd('crti.c', 'crti.bc', bias_arch,
                                    output_dir=libc_libdir),
              # Install crtbegin bitcode (__cxa_finalize for C++)
              BuildTargetBitcodeCmd('crtbegin.c', 'crtbegin.bc', bias_arch,
                                    output_dir=clang_libdir),
              # Stubs for _Unwind_* functions when libgcc_eh is not included in
              # the native link).
              BuildTargetBitcodeCmd('unwind_stubs.c', 'unwind_stubs.bc',
                                    bias_arch, output_dir=clang_libdir),
              BuildTargetBitcodeCmd('sjlj_eh_redirect.cc',
                                    'sjlj_eh_redirect.bc', bias_arch,
                                    output_dir=clang_libdir),
              # libpnaclmm.a (__atomic_* library functions).
              BuildTargetBitcodeCmd('pnaclmm.c', 'pnaclmm.bc', bias_arch),
              command.Command([
                  PnaclTool('ar'), 'rc',
                  command.path.join(clang_libdir, 'libpnaclmm.a'),
                  'pnaclmm.bc']),
          ],
      },
  }
  return libs


def NativeLibs(arch, is_canonical):
  setjmp_arch = arch
  if setjmp_arch.endswith('-nonsfi'):
    setjmp_arch = setjmp_arch[:-len('-nonsfi')]

  arch_cmds = []
  if arch == 'arm':
    arch_cmds.append(
        BuildTargetNativeCmd('aeabi_read_tp.S', 'aeabi_read_tp.o', arch))
  elif arch == 'x86-32-nonsfi':
    arch_cmds.extend(
        [BuildTargetNativeCmd('entry_linux.c', 'entry_linux.o', arch),
         BuildTargetNativeCmd('entry_linux_x86_32.S', 'entry_linux_asm.o',
                              arch)])
  elif arch == 'arm-nonsfi':
    arch_cmds.extend(
        [BuildTargetNativeCmd('entry_linux.c', 'entry_linux.o', arch),
         BuildTargetNativeCmd('entry_linux_arm.S', 'entry_linux_asm.o',
                              arch)])
  native_lib_dir = os.path.join('translator', arch, 'lib')

  libs = {
      GSDJoin('libs_support_native', arch): {
          'type': 'build' if is_canonical else 'work',
          'output_subdir': native_lib_dir,
          'dependencies': [ 'newlib_src', 'newlib_le32',
                            'target_lib_compiler'],
          # These libs include
          # arbitrary stuff from native_client/src/{include,untrusted,trusted}
          'inputs': { 'src': os.path.join(NACL_DIR, 'pnacl', 'support'),
                      'include': os.path.join(NACL_DIR, 'src'),
                      'newlib_subset': os.path.join(
                          NACL_DIR, 'src', 'third_party',
                          'pnacl_native_newlib_subset')},
          'commands': [
              BuildTargetNativeCmd('crtbegin.c', 'crtbegin.o', arch,
                                   output_dir='%(output)s'),
              BuildTargetNativeCmd('crtbegin.c', 'crtbegin_for_eh.o', arch,
                                   ['-DLINKING_WITH_LIBGCC_EH'],
                                   output_dir='%(output)s'),
              BuildTargetNativeCmd('crtend.c', 'crtend.o', arch,
                                   output_dir='%(output)s'),
              # libcrt_platform.a
              BuildTargetNativeCmd('pnacl_irt.c', 'pnacl_irt.o', arch),
              BuildTargetNativeCmd('relocate.c', 'relocate.o', arch),
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
              arch_cmds + [
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
      GSDJoin('compiler_rt', arch): {
          'type': 'build' if is_canonical else 'work',
          'output_subdir': native_lib_dir,
          'dependencies': ['compiler_rt_src', 'target_lib_compiler',
                           'newlib_le32'],
          'commands': [
              command.Command(MakeCommand() + [
                  '-f',
                  command.path.join('%(compiler_rt_src)s', 'lib',
                                    'Makefile-pnacl'),
                  'libgcc.a', 'CC=' + PnaclTool('clang'),
                  'AR=' + PnaclTool('ar')] +
                  ['SRC_DIR=' + command.path.join('%(abs_compiler_rt_src)s',
                                                  'lib'),
                   'CFLAGS=-arch ' + arch + ' -DPNACL_' +
                    arch.replace('-', '_') + ' --pnacl-allow-translate -O3 ' +
                   NewlibIsystemCflags('le32')]),
              command.Copy('libgcc.a', os.path.join('%(output)s', 'libgcc.a')),
          ],
      },
  }
  if not arch.endswith('-nonsfi'):
    libs.update({
      GSDJoin('libgcc_eh', arch): {
          'type': 'build' if is_canonical else 'work',
          'output_subdir': native_lib_dir,
          'dependencies': [ 'gcc_src', 'target_lib_compiler'],
          'inputs': { 'scripts': os.path.join(NACL_DIR, 'pnacl', 'scripts')},
          'commands': [
              # Instead of trying to use gcc's build system to build only
              # libgcc_eh, we just build the C files and archive them manually.
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
      GSDJoin('unsandboxed_irt', arch): {
          'type': 'build',
          'output_subdir': os.path.join('translator', arch, 'lib'),
          # This lib #includes
          # arbitrary stuff from native_client/src/{include,untrusted,trusted}
          'inputs': { 'support': os.path.join(NACL_DIR, 'src', 'nonsfi', 'irt'),
                      'untrusted': os.path.join(
                          NACL_DIR, 'src', 'untrusted', 'irt'),
                      'include': os.path.join(NACL_DIR, 'src'), },
          'commands': [
              # The NaCl headers insist on having a platform macro such as
              # NACL_LINUX defined, but src/nonsfi/irt_interfaces.c does not
              # itself use any of these macros, so defining NACL_LINUX here
              # even on non-Linux systems is OK.
              # TODO(dschuff): this include path breaks the input encapsulation
              # for build rules.
              command.Command([
                  'gcc', '-m32', '-O2', '-Wall', '-Werror',
                  '-I%(top_srcdir)s/..', '-DNACL_LINUX=1', '-DDEFINE_MAIN',
                  '-c', command.path.join('%(support)s', 'irt_interfaces.c'),
                  '-o', command.path.join('%(output)s', 'unsandboxed_irt.o')]),
              command.Command([
                  'gcc', '-m32', '-O2', '-Wall', '-Werror',
                  '-I%(top_srcdir)s/..',
                  '-c', command.path.join('%(untrusted)s', 'irt_query_list.c'),
                  '-o', command.path.join('%(output)s', 'irt_query_list.o')]),
          ],
      },
  }
  return libs

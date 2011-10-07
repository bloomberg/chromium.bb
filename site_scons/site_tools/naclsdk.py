#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Nacl SDK tool SCons."""

import __builtin__
import re
import os
import shutil
import sys
import SCons.Scanner
import SCons.Script
import subprocess


# Mapping from env['PLATFORM'] (scons) to a single host platform from the point
# of view of NaCl.
NACL_CANONICAL_PLATFORM_MAP = {
    'win32': 'win',
    'cygwin': 'win',
    'posix': 'linux',
    'linux': 'linux',
    'linux2': 'linux',
    'darwin': 'mac',
}

# NACL_PLATFORM_DIR_MAP is a mapping from the canonical platform,
# BUILD_ARCHITECHTURE and BUILD_SUBARCH to a subdirectory name in the
# download target directory. See _GetNaclSdkRoot below.
NACL_PLATFORM_DIR_MAP = {
    'win': {
        'x86': {
            '32': 'win_x86',
            '64': 'win_x86',
        },
    },
    'linux': {
        'x86': {
            '32': 'linux_x86',
            '64': 'linux_x86',
        },
    },
    'mac': {
        'x86': {
            '32': 'mac_x86',
            '64': 'mac_x86',
        },
    },
}

def _PlatformSubdirs(env):
  if env.Bit('bitcode'):
    import platform
    machine = platform.machine()
    # Make Windows python consistent with Cygwin python
    if machine == 'x86':
      machine = 'i686'
    name = 'pnacl_%s_%s' % (platform.system().lower(), machine)
    if env.Bit('nacl_glibc'):
      name += '_glibc'
    else:
      name += '_newlib'
  else:
    platform = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
    arch = env['BUILD_ARCHITECTURE']
    subarch = env['TARGET_SUBARCH']
    name = NACL_PLATFORM_DIR_MAP[platform][arch][subarch]
    if not env.Bit('nacl_glibc'):
      name = name + '_newlib'
  return name


def _GetNaclSdkRoot(env, sdk_mode):
  """Return the path to the sdk.

  Args:
    env: The SCons environment in question.
    sdk_mode: A string indicating which location to select the tools from.
  Returns:
    The path to the sdk.
  """
  if sdk_mode == 'local':
    if env['PLATFORM'] in ['win32', 'cygwin']:
      # Try to use cygpath under the assumption we are running thru cygwin.
      # If this is not the case, then 'local' doesn't really make any sense,
      # so then we should complain.
      try:
        path = subprocess.Popen(
            ['cygpath', '-m', '/usr/local/nacl-sdk'],
            env={'PATH': os.environ['PRESCONS_PATH']}, shell=True,
            stdout=subprocess.PIPE).communicate()[0].replace('\n', '')
      except WindowsError:
        raise NotImplementedError(
            'Not able to decide where /usr/local/nacl-sdk is on this platform,'
            'use naclsdk_mode=custom:...')
      return path
    else:
      return '/usr/local/nacl-sdk'

  elif sdk_mode == 'download':
    tcname = _PlatformSubdirs(env)
    return os.path.join(env['MAIN_DIR'], 'toolchain', tcname)
  elif sdk_mode.startswith('custom:'):
    return os.path.abspath(sdk_mode[len('custom:'):])

  elif sdk_mode == 'manual':
    return None

  else:
    raise Exception('Unknown sdk mode: %r' % sdk_mode)


def _SetEnvForX86Sdk(env, sdk_path):
  """Initialize environment according to target architecture."""

  bin_path = os.path.join(sdk_path, 'bin')
  # NOTE: attempts to eliminate this PATH setting and use
  #       absolute path have been futile
  env.PrependENVPath('PATH', bin_path)

  if os.path.exists(os.path.join(sdk_path, 'nacl64')):
    arch = 'nacl64'
    default_subarch = '64'
  elif os.path.exists(os.path.join(sdk_path, 'x86_64-nacl')):
    arch = 'x86_64-nacl'
    default_subarch = '64'
  else:
    # This fallback allows the Scons build to work if we have a
    # 32-bit-by-default toolchain that lacks "nacl64" compatibility
    # symlinks.
    arch = 'nacl'
    default_subarch = '32'

  # Although "lib32" is symlinked to "lib/32" and "lib64" is symlinked
  # to "lib", we use the actual directories because, on Windows, Scons
  # does not run under Cygwin and does not follow Cygwin symlinks.
  if env['TARGET_SUBARCH'] == default_subarch:
    libsuffix = 'lib'
    cc_mode_flag = ''
    as_mode_flag = ''
    ld_mode_flag = ''
  else:
    libsuffix = 'lib%s' % env['TARGET_SUBARCH']
    cc_mode_flag = ' -m%s' % env['TARGET_SUBARCH']
    as_mode_flag = ' --%s' % env['TARGET_SUBARCH']
    if env['TARGET_SUBARCH'] == '64':
      ld_mode_flag = ' -melf64_nacl'
    else:
      ld_mode_flag = ' -melf_nacl'

  env.Replace(# Replace header and lib paths.
              # where to put nacl extra sdk headers
              # TODO(robertm): switch to using the mechanism that
              #                passes arguments to scons
              NACL_SDK_INCLUDE='%s/%s/include' % (sdk_path, arch),
              # where to find/put nacl generic extra sdk libraries
              NACL_SDK_LIB='%s/%s/%s' % (sdk_path, arch, libsuffix),
              # Replace the normal unix tools with the NaCl ones.
              CC=os.path.join(bin_path, '%s-gcc%s' % (arch, cc_mode_flag)),
              CXX=os.path.join(bin_path, '%s-g++%s' % (arch, cc_mode_flag)),
              AR=os.path.join(bin_path, '%s-ar' % arch),
              AS=os.path.join(bin_path, '%s-as%s' % (arch, as_mode_flag)),
              GDB=os.path.join(bin_path, 'nacl-gdb'),
              # NOTE: use g++ for linking so we can handle C AND C++.
              LINK=os.path.join(bin_path, '%s-g++%s' % (arch, cc_mode_flag)),
              # Grrr... and sometimes we really need ld.
              LD=os.path.join(bin_path, '%s-ld%s' % (arch, ld_mode_flag)),
              RANLIB=os.path.join(bin_path, '%s-ranlib' % arch),
              OBJDUMP=os.path.join(bin_path, '%s-objdump' % arch),
              STRIP=os.path.join(bin_path, '%s-strip' % arch),
              # Strip doesn't seem to be a first-class citizen in SCons country,
              # so we have to add these *COM, *COMSTR manually.
              # Note: it appears we cannot add this in component_setup.py
              # to avoid duplication =(. Thus, this is duplicated here,
              # and in the pnacl sdk setup.
              STRIPFLAGS=['--strip-all'],
              STRIPCOM='${STRIP} ${STRIPFLAGS}',
              CFLAGS = ['-std=gnu99'],
              CCFLAGS=['-O3',
                       '-Werror',
                       '-Wall',
                       '-Wswitch-enum',
                       '-g',
                       '-fno-stack-protector',
                       '-fdiagnostics-show-option',
                       '-pedantic',
                       '-D__linux__',
                       ],
              ASFLAGS=[],
              )

  # NaClSdk environment seems to be inherited from the host environment.
  # On Linux host, this probably makes sense. On Windows and Mac, this
  # introduces nothing except problems.
  # For now, simply override the environment settings as in
  # <scons>/engine/SCons/Platform/posix.py
  env.Replace(LIBPREFIX='lib',
              LIBSUFFIX='.a',
              SHLIBPREFIX='$LIBPREFIX',
              SHLIBSUFFIX='.so',
              LIBPREFIXES=['$LIBPREFIX'],
              LIBSUFFIXES=['$LIBSUFFIX', '$SHLIBSUFFIX'],
              )
  # Force -fPIC when compiling for shared libraries.
  env.AppendUnique(SHCCFLAGS=['-fPIC'],
                   )

def _SetEnvForPnacl(env, root):
  arch = env['TARGET_FULLARCH']
  assert arch in ['arm', 'arm-thumb2', 'x86-32', 'x86-64']

  arch_flag = ' -arch %s' % arch

  binprefix = os.path.join(root, 'bin', 'pnacl-')
  binext = ''
  if env.Bit('host_windows'):
    binext = '.bat'

  pnacl_lib = os.path.join(root, 'lib')
  pnacl_lib_arch = os.path.join(root, 'lib-%s' % arch)

  #TODO(robertm): remove NACL_SDK_INCLUDE ASAP
  pnacl_include = os.path.join(root, 'sysroot', 'include')
  pnacl_ar = binprefix + 'ar' + binext
  pnacl_nm = binprefix + 'nm' + binext
  pnacl_ranlib = binprefix + 'ranlib' + binext

  frontend = env['PNACL_FRONTEND']
  if frontend == 'clang':
    pnacl_cc = binprefix + 'clang' + binext
    pnacl_cxx = binprefix + 'clang++' + binext
  elif frontend == 'llvmgcc':
    pnacl_cc = binprefix + 'gcc' + binext
    pnacl_cxx = binprefix + 'g++' + binext
  elif frontend == 'dragonegg':
    pnacl_cc = binprefix + 'dgcc' + binext
    pnacl_cxx = binprefix + 'dg++' + binext
  else:
    print "Unknown frontend"
    sys.exit(-1)

  pnacl_ld = binprefix + 'ld' + binext
  pnacl_disass = binprefix + 'dis' + binext
  pnacl_strip = binprefix + 'strip' + binext

  # NOTE: XXX_flags start with space for easy concatenation
  # The flags generated here get baked into the commands (CC, CXX, LINK)
  # instead of CFLAGS etc to keep them from getting blown away by some
  # tests. Don't add flags here unless they always need to be preserved.
  pnacl_cxx_flags = ''
  pnacl_cc_flags = ' -std=gnu99'
  pnacl_cc_native_flags = ' -std=gnu99' + arch_flag
  pnacl_ld_flags = ' ' + ' '.join(env['PNACL_BCLDFLAGS'])

  if env.Bit('nacl_pic'):
    pnacl_cc_flags += ' -fPIC'
    pnacl_cxx_flags += ' -fPIC'
    # NOTE: this is a special hack for the pnacl backend which
    #       does more than linking
    pnacl_ld_flags += ' -fPIC'

  if env.Bit('use_sandboxed_translator'):
    pnacl_ld_flags += ' --pnacl-sb'
    if env.Bit('sandboxed_translator_is_dynamic'):
      pnacl_ld_flags += ' --pnacl-sb-dynamic'

  # TODO(pdox): Remove PNaCl's dependency on the gcc toolchain here.
  platform = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
  nnacl_root = os.path.join(env['MAIN_DIR'], 'toolchain', '%s_x86' % platform)

  # NACL_SDK_LIB is prepended to LIBPATH in generate()
  env.Prepend(LIBPATH=pnacl_lib)

  env.Replace(# Replace header and lib paths.
              PNACL_ROOT=root,
              NACL_SDK_INCLUDE=pnacl_include,
              NACL_SDK_LIB=pnacl_lib_arch,
              # Replace the normal unix tools with the PNaCl ones.
              CC=pnacl_cc + pnacl_cc_flags,
              CXX=pnacl_cxx + pnacl_cxx_flags,
              LIBPREFIX="lib",
              SHLIBPREFIX="lib",
              SHLIBSUFFIX=".pso",
              OBJSUFFIX=".bc",
              LINK=pnacl_cxx + arch_flag + pnacl_ld_flags,
              # Although we are currently forced to produce native output
              # for LINK, we are free to produce bitcode for SHLINK
              # (SharedLibrary linking) because scons doesn't do anything
              # with shared libraries except use them with the toolchain.
              SHLINK=pnacl_cxx + pnacl_ld_flags,
              # C_ONLY_LINK is needed when building libehsupport,
              # because libstdc++ is not yet available.
              C_ONLY_LINK=pnacl_cc + arch_flag + pnacl_ld_flags,
              LD=pnacl_ld,
              AR=pnacl_ar,
              RANLIB=pnacl_ranlib,
              DISASS=pnacl_disass,
              OBJDUMP=pnacl_disass,
              STRIP=pnacl_strip,
              # Strip doesn't seem to be a first-class citizen in SCons country,
              # so we have to add these *COM, *COMSTR manually.
              # Note: it appears we cannot add this in component_setup.py
              # to avoid duplication =( Thus, this is duplicated here,
              # and in the nacl-gcc setup.
              STRIPFLAGS=['--strip-all'],
              STRIPCOM='${STRIP} ${STRIPFLAGS}',
              )


def _SetEnvForSdkManually(env):
  def GetEnvOrDummy(v):
    return os.getenv('NACL_SDK_' + v, 'MISSING_SDK_' + v)

  env.Replace(# Replace header and lib paths.
              NACL_SDK_INCLUDE=GetEnvOrDummy('INCLUDE'),
              NACL_SDK_LIB=GetEnvOrDummy('LIB'),
              # Replace the normal unix tools with the NaCl ones.
              CC=GetEnvOrDummy('CC'),
              CXX=GetEnvOrDummy('CXX'),
              AR=GetEnvOrDummy('AR'),
              # NOTE: use g++ for linking so we can handle c AND c++
              LINK=GetEnvOrDummy('LINK'),
              RANLIB=GetEnvOrDummy('RANLIB'),
              )

def PNaClForceNative(env):
  assert(env.Bit('bitcode'))
  env.Replace(OBJSUFFIX='.o',
              SHLIBSUFFIX='.so')
  env.Append(ASFLAGS=['-arch', '${TARGET_FULLARCH}'],
             CCFLAGS=['-arch', '${TARGET_FULLARCH}', '--pnacl-allow-translate'],
             LINKFLAGS=['--pnacl-allow-native'])
  env['SHLINK'] = '${LINK}'

# Get an environment for nacl-gcc when in PNaCl mode.
def PNaClGetNNaClEnv(env):
  assert(env.Bit('bitcode'))
  assert(not env.Bit('target_arm'))

  # This is kind of a hack. We clone the environment,
  # clear the bitcode bit, and then reload naclsdk.py
  native_env = env.Clone()
  native_env.ClearBits('bitcode')
  native_env = native_env.Clone(tools = ['naclsdk'])
  return native_env


# This adds architecture specific defines for the target architecture.
# These are normally omitted by PNaCl.
# For example: __i686__, __arm__, __x86_64__
def AddBiasForPNaCl(env):
  assert(env.Bit('bitcode'))

  if env.Bit('target_arm'):
    env.AppendUnique(CCFLAGS=['--pnacl-arm-bias'],
                     CXXFLAGS=['--pnacl-arm-bias'])
  elif env.Bit('target_x86_32'):
    env.AppendUnique(CCFLAGS=['--pnacl-i686-bias'],
                     CXXFLAGS=['--pnacl-i686-bias'])
  elif env.Bit('target_x86_64'):
    env.AppendUnique(CCFLAGS=['--pnacl-x86_64-bias'],
                     CXXFLAGS=['--pnacl-x86_64-bias'])
  else:
    raise Exception("Unknown architecture!")


def ValidateSdk(env):
  checkables = ['${NACL_SDK_INCLUDE}/stdio.h']
  for c in checkables:
    if os.path.exists(env.subst(c)):
      continue
    # Windows build does not use cygwin and so can not see nacl subdirectory
    # if it's cygwin's symlink - check for /include instead...
    if os.path.exists(re.sub(r'(nacl64|nacl)/include/([^/]*)$',
                             r'include/\2',
                             env.subst(c))):
      continue
    # TODO(pasko): remove the legacy header presence test below.
    if os.path.exists(re.sub(r'nacl/include/([^/]*)$',
                             r'nacl64/include/\1',
                             env.subst(c))):
      continue
    message = env.subst('''
ERROR: NativeClient toolchain does not seem present!,
       Missing: %s

Configuration is:
  NACL_SDK_INCLUDE=${NACL_SDK_INCLUDE}
  NACL_SDK_LIB=${NACL_SDK_LIB}
  CC=${CC}
  CXX=${CXX}
  AR=${AR}
  AS=${AS}
  LINK=${LINK}
  RANLIB=${RANLIB}

Run: gclient runhooks --force or build the SDK yourself.
''' % c)
    sys.stderr.write(message + "\n\n")
    sys.exit(-1)


def ScanLinkerScript(node, env, libpath):
  """SCons scanner for linker script files.
This handles trivial linker scripts like those used for libc.so and libppapi.a.
These scripts just indicate more input files to be linked in, so we want
to produce dependencies on them.

A typical such linker script looks like:

        /* Some comments.  */
        INPUT ( foo.a libbar.a libbaz.a )

or:

        /* GNU ld script
           Use the shared library, but some functions are only in
           the static library, so try that secondarily.  */
        OUTPUT_FORMAT(elf64-x86-64)
        GROUP ( /lib/libc.so.6 /usr/lib/libc_nonshared.a
                AS_NEEDED ( /lib/ld-linux-x86-64.so.2 ) )
"""
  contents = node.get_text_contents()
  if contents.startswith('!<arch>\n') or contents.startswith('\177ELF'):
    # An archive or ELF file is not a linker script.
    return []

  comment_pattern = re.compile(r'/\*.*?\*/', re.DOTALL | re.MULTILINE)
  def remove_comments(text):
    return re.sub(comment_pattern, '', text)

  tokens = remove_comments(contents).split()
  libs = []
  while tokens:
    token = tokens.pop()
    if token.startswith('OUTPUT_FORMAT('):
      pass
    elif token == 'OUTPUT_FORMAT':
      # Swallow the next three tokens: '(', 'xyz', ')'
      del tokens[0:2]
    elif token in ['(', ')', 'INPUT', 'GROUP', 'AS_NEEDED']:
      pass
    else:
      libs.append(token)

  # Find those items in the library path, ignoring ones we fail to find.
  found = [SCons.Node.FS.find_file(lib, libpath) for lib in libs]
  return [lib for lib in found if lib is not None]

def generate(env):
  """SCons entry point for this tool.

  Args:
    env: The SCons environment in question.

  NOTE: SCons requires the use of this name, which fails lint.
  """

  # make these methods to the top level scons file
  env.AddMethod(ValidateSdk)
  env.AddMethod(AddBiasForPNaCl)
  env.AddMethod(PNaClForceNative)
  env.AddMethod(PNaClGetNNaClEnv)

  sdk_mode = SCons.Script.ARGUMENTS.get('naclsdk_mode', 'download')

  # Invoke the various unix tools that the NativeClient SDK resembles.
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  env.Replace(
      COMPONENT_LINKFLAGS=[''],
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.pso', '.so', '.a'],
      _RPATH='',
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=[],
      PROGSUFFIX='.nexe'
  )

  # Get root of the SDK.
  root = _GetNaclSdkRoot(env, sdk_mode)

  # Determine where to get the SDK from.
  if sdk_mode == 'manual':
    _SetEnvForSdkManually(env)
  else:
    # if bitcode=1 use pnacl toolchain
    if env.Bit('bitcode'):
      _SetEnvForPnacl(env, root)
    elif env.Bit('target_x86'):
      _SetEnvForX86Sdk(env, root)
    else:
      raise Exception('ERROR: unknown TARGET_ARCHITECTURE: %s'
                      % env['TARGET_ARCHITECTURE'])

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

  # Install our scanner for (potential) linker scripts.
  # It applies to "source" files ending in .a or .so.
  # Dependency files it produces are to be found in ${LIBPATH}.
  # It is applied recursively to those dependencies in case
  # some of them are linker scripts too.
  ldscript_scanner = SCons.Scanner.Base(
      function=ScanLinkerScript,
      skeys=['.a', '.so'],
      path_function=SCons.Scanner.FindPathDirs('LIBPATH'),
      recursive=True
      )
  env.Append(SCANNERS=ldscript_scanner)

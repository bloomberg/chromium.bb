#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Nacl SDK tool SCons."""

import __builtin__
import re
import os
import shutil
import sys
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
    name = 'pnacl_%s_%s' % (platform.system().lower(), platform.machine())
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
              CFLAGS = ['-std=gnu99'],
              CCFLAGS=['-O3',
                       '-Werror',
                       '-Wall',
                       '-Wswitch-enum',
                       '-g',
                       '-fno-builtin',
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
  assert arch in ['arm', 'x86-32', 'x86-64']

  env['PNACL_ROOT'] = root
  pnacl_sdk_lib = '${PNACL_ROOT}/libs-bitcode'
  #TODO(robertm): remove NACL_SDK_INCLUDE ASAP
  pnacl_sdk_include = '${PNACL_ROOT}/arm-newlib/arm-none-linux-gnueabi/include'
  pnacl_sdk_ar = '${PNACL_ROOT}/bin/pnacl-ar'
  pnacl_sdk_nm = '${PNACL_ROOT}/bin/pnacl-nm'
  pnacl_sdk_ranlib = '${PNACL_ROOT}/bin/pnacl-ranlib'

  pnacl_sdk_cc = '${PNACL_ROOT}/bin/pnacl-gcc'
  pnacl_sdk_cxx = '${PNACL_ROOT}/bin/pnacl-g++'
  pnacl_sdk_ld =  '${PNACL_ROOT}/bin/pnacl-ld'
  pnacl_sdk_disass = '${PNACL_ROOT}/bin/pnacl-dis'
  # NOTE: XXX_flags start with space for easy concatenation
  pnacl_sdk_cxx_flags = ''
  pnacl_sdk_cc_flags = ' -std=gnu99'
  pnacl_sdk_cc_native_flags = ' -std=gnu99 -arch %s' % arch
  pnacl_sdk_ld_flags = ' -arch %s' % arch
  pnacl_sdk_ld_flags += ' ' + ' '.join(env['PNACL_BCLDFLAGS'])

  if env.Bit('nacl_pic'):
    pnacl_sdk_cc_flags += ' -fPIC'
    pnacl_sdk_cxx_flags += ' -fPIC'
    # NOTE: this is a special hack for the pnacl backend which
    #       does more than linking
    pnacl_sdk_ld_flags += ' -fPIC'

  if env.Bit('use_sandboxed_translator'):
    pnacl_sdk_ld_flags += ' --pnacl-sb'

  # TODO(pdox): Remove PNaCl's dependency on the gcc toolchain here.
  platform = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
  nnacl_root = os.path.join(env['MAIN_DIR'], 'toolchain', '%s_x86' % platform)

  env.Replace(# Replace header and lib paths.
              NACL_SDK_INCLUDE=pnacl_sdk_include,
              NACL_SDK_LIB=pnacl_sdk_lib,
              # Replace the normal unix tools with the PNaCl ones.
              CC=pnacl_sdk_cc + pnacl_sdk_cc_flags,
              CXX=pnacl_sdk_cxx + pnacl_sdk_cxx_flags,
              OBJSUFFIX=".bc",
              LINK=pnacl_sdk_ld + pnacl_sdk_ld_flags,
              LD=pnacl_sdk_ld,
              AR=pnacl_sdk_ar,
              RANLIB=pnacl_sdk_ranlib,
              DISASS=pnacl_sdk_disass,
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

# This adds architecture specific defines for the target architecture.
# These are normally omitted by PNaCl.
# For example: __i686__, __arm__, __x86_64__
def AddBiasForPNaCl(env):
  assert(env.Bit('bitcode'))

  if env.Bit('target_arm'):
    env.Append(CCFLAGS=['--pnacl-arm-bias'])
    env.Append(CXXFLAGS=['--pnacl-arm-bias'])
  elif env.Bit('target_x86_32'):
    env.Append(CCFLAGS=['--pnacl-i686-bias'])
    env.Append(CXXFLAGS=['--pnacl-i686-bias'])
  elif env.Bit('target_x86_64'):
    env.Append(CCFLAGS=['--pnacl-x86_64-bias'])
    env.Append(CXXFLAGS=['--pnacl-x86_64-bias'])
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


def generate(env):
  """SCons entry point for this tool.

  Args:
    env: The SCons environment in question.

  NOTE: SCons requires the use of this name, which fails lint.
  """

  # make these methods to the top level scons file
  env.AddMethod(ValidateSdk)
  env.AddMethod(AddBiasForPNaCl)

  sdk_mode = SCons.Script.ARGUMENTS.get('naclsdk_mode', 'download')

  # Invoke the various unix tools that the NativeClient SDK resembles.
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  env.Replace(
      COMPONENT_LINKFLAGS=[''],
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.so', '.a'],
      _RPATH='',
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=[],

      # TODO: This is needed for now to work around unc paths.  Take this out
      # when unc paths are fixed.
      IMPLICIT_COMMAND_DEPENDENCIES=False,

      # TODO: this could be .nexe and then all the .nexe stuff goes away?
      PROGSUFFIX=''  # Force PROGSUFFIX to '' on all platforms.
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
      print "ERROR: unknown TARGET_ARCHITECTURE: ", env['TARGET_ARCHITECTURE']
      assert 0

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

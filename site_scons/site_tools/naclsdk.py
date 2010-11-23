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
            '32': ['win_x86'],
            '64': ['win_x86'],
        },
    },
    'linux': {
        'x86': {
            '32': ['linux_x86'],
            '64': ['linux_x86'],
        },
        'arm': {
            '32': ['linux_arm-untrusted', 'linux_arm-trusted'],
        },
    },
    'mac': {
        'x86': {
            '32': ['mac_x86'],
            '64': ['mac_x86'],
        },
    },
}



def _PlatformSubdirs(env):
  platform = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
  arch = env['BUILD_ARCHITECTURE']
  subarch = env['TARGET_SUBARCH']
  name = NACL_PLATFORM_DIR_MAP[platform][arch][subarch]
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
    platforms = _PlatformSubdirs(env)
    root = os.path.join(env['MAIN_DIR'], 'toolchain', platforms[-1])
    return root

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
  else:
    libsuffix = 'lib/%s' % env['TARGET_SUBARCH']
  extraflag = '-m%s' % env['TARGET_SUBARCH']

  env.Replace(# Replace header and lib paths.
              # where to put nacl extra sdk headers
              # TODO(robertm): switch to using the mechanism that
              #                passes arguments to scons
              NACL_SDK_INCLUDE='%s/%s/include' % (sdk_path, arch),
              # where to find/put nacl generic extra sdk libraries
              NACL_SDK_LIB='%s/%s/%s' % (sdk_path, arch, libsuffix),
              # Replace the normal unix tools with the NaCl ones.
              CC=os.path.join(bin_path, '%s-gcc' % arch),
              CXX=os.path.join(bin_path, '%s-g++' % arch),
              AR=os.path.join(bin_path, '%s-ar' % arch),
              AS=os.path.join(bin_path, '%s-as' % arch),
              GDB=os.path.join(bin_path, 'nacl-gdb'),
              # NOTE: use g++ for linking so we can handle C AND C++.
              LINK=os.path.join(bin_path, '%s-g++' % arch),
              # Grrr... and sometimes we really need ld.
              LD=os.path.join(bin_path, '%s-ld' % arch),
              RANLIB=os.path.join(bin_path, '%s-ranlib' % arch),
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
                       extraflag,
                       ],
              ASFLAGS=[extraflag],
              )

def _SetEnvForPnacl(env, arch):
  assert arch in ['arm', 'x86-32', 'x86-64']
  pnacl_sdk_root = '${MAIN_DIR}/toolchain/linux_arm-untrusted'
  pnacl_sdk_lib = pnacl_sdk_root + '/libs-bitcode'
  #TODO(robertm): remove NACL_SDK_INCLUDE ASAP
  pnacl_sdk_include = (pnacl_sdk_root +
                       '/arm-newlib/arm-none-linux-gnueabi/include')
  pnacl_sdk_ar = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/bin/arm-none-linux-gnueabi-ar')
  pnacl_sdk_nm = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/bin/arm-none-linux-gnueabi-nm')
  pnacl_sdk_ranlib = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                      '/bin/arm-none-linux-gnueabi-ranlib')

  pnacl_sdk_cc = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/llvm-fake-sfigcc')
  pnacl_sdk_cxx = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/llvm-fake-sfig++')
  pnacl_sdk_ld =  (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/llvm-fake-bcld')
  pnacl_sdk_disass = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/bin/llvm-dis')
  # NOTE: XXX_flags start with space for easy concatenation
  pnacl_sdk_cxx_flags = ' -emit-llvm'
  pnacl_sdk_cc_flags = ' -emit-llvm  -std=gnu99'
  pnacl_sdk_cc_native_flags = ' -std=gnu99 -arch %s' % arch
  pnacl_sdk_ld_flags = ' -arch %s' % arch
  pnacl_sdk_ld_flags += ' ' + ' '.join(env['PNACL_BCLDFLAGS'])

  cc_other_map = {
      'arm':    pnacl_sdk_cc + pnacl_sdk_cc_native_flags,
      'x86-32': '${MAIN_DIR}/toolchain/linux_x86/bin/nacl-gcc',
      'x86-64': '${MAIN_DIR}/toolchain/linux_x86/bin/nacl64-gcc'
      }
  ld_map = {
      'arm':    (pnacl_sdk_root + '/arm-none-linux-gnueabi/bin/' +
                 'arm-none-linux-gnueabi-ld'),
      'x86-32': '${MAIN_DIR}/toolchain/linux_x86/bin/nacl-ld',
      'x86-64': '${MAIN_DIR}/toolchain/linux_x86/bin/nacl64-ld'
        }

  env.Replace(# Replace header and lib paths.
              NACL_SDK_INCLUDE=pnacl_sdk_include,
              NACL_SDK_LIB=pnacl_sdk_lib,
              # Replace the normal unix tools with the PNaCl ones.
              CC=pnacl_sdk_cc + pnacl_sdk_cc_flags,
              CXX=pnacl_sdk_cxx + pnacl_sdk_cxx_flags,
              # NOTE: only in bitcode compilation scenarios where
              #       CC compiles to bitcode and
              #       CC_NATIVE compiles to native code
              #       (CC_NATIVE had to handle both .c and .S files)
              CC_NATIVE=pnacl_sdk_cc + pnacl_sdk_cc_native_flags,
              CC_OTHER=cc_other_map[arch],
              LINK=pnacl_sdk_ld + pnacl_sdk_ld_flags,
              # Grrr... and sometimes we need raw assembly and the real ld.
              LD=ld_map[arch],
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
              # NOTE: only used in bitcode compilation scenarios where
              #       CC compiles to bitcode and
              #       CC_NATIVE compiles to native code
              #       (CC_NATIVE has to handle both .c and .S files)
              CC_NATIVE=GetEnvOrDummy('CC_NATIVE'),
              # NOTE: CC_OTHER is used in bitcode compilation scenarios
              #       where CC compiles to bitcode and we need to build
              #       native assembly code source code, typically for
              #       low-level tests.  We cannot use CC_NATIVE because
              #       CC_NATIVE on x86 does not handle .S files propery.
              #       See
              # http://code.google.com/p/nativeclient/issues/detail?id=1182
              CC_OTHER=GetEnvOrDummy('CC_OTHER'),
              # NOTE: use g++ for linking so we can handle c AND c++
              LINK=GetEnvOrDummy('LINK'),
              RANLIB=GetEnvOrDummy('RANLIB'),
              )

def ValidateSdk(env):
  checkables = ['${NACL_SDK_INCLUDE}/stdio.h']
  for c in checkables:
    if os.path.exists(env.subst(c)):
      continue
    # Windows build does not use cygwin and so can not see nacl subdirectory
    # if it's cygwin's symlink - check for nacl64/include instead...
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

  sdk_mode = SCons.Script.ARGUMENTS.get('naclsdk_mode', 'download')

  # Invoke the various unix tools that the NativeClient SDK resembles.
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  env.Replace(
      HOST_PLATFORMS=['*'],  # NaCl builds on all platforms.

      COMPONENT_LINKFLAGS=[''],
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.a'],
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
      _SetEnvForPnacl(env, env['TARGET_FULLARCH'])
    elif env['TARGET_ARCHITECTURE'] == 'x86':
      _SetEnvForX86Sdk(env, root)
    else:
      print "ERROR: unknown TARGET_ARCHITECTURE: ", env['TARGET_ARCHITECTURE']
      assert 0

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

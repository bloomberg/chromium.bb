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

  # NOTE: attempts to eliminate this PATH setting and use
  #       absolute path have been futile
  env.PrependENVPath('PATH', sdk_path + '/bin')

  # /lib64 is symlinked to /lib, we are using the actual directory because Scons
  # does not work correctly with Cygwin symlinks.
  libsuffix = '/lib'
  extraflag = '-m64'
  if env['TARGET_SUBARCH'] == '32':
    # /lib32 is symlinked to /lib/32.
    libsuffix = '/lib/32'
    extraflag = '-m32'
  elif env['TARGET_SUBARCH'] != '64':
    print "ERROR: unknown TARGET_SUBARCH: ", env['TARGET_SUBARCH']
    assert 0

  env.Replace(# Replace header and lib paths.
              # where to put nacl extra sdk headers
              # TODO(robertm): switch to using the mechanism that
              #                passes arguments to scons
              NACL_SDK_INCLUDE=sdk_path + '/nacl64/include',
              # where to find/put nacl generic extra sdk libraries
              NACL_SDK_LIB=sdk_path + '/nacl64' + libsuffix,
              # Replace the normal unix tools with the NaCl ones.
              CC='nacl64-gcc',
              CXX='nacl64-g++',
              AR='nacl64-ar',
              AS='nacl64-as',
              GDB='nacl-gdb',
              # NOTE: use g++ for linking so we can handle C AND C++.
              LINK='nacl64-g++',
              RANLIB='nacl64-ranlib',
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
  if env.Bit('nacl_glibc'):
    env.Replace(CC="nacl-glibc-gcc")
    env.Replace(LINK="nacl-glibc-gcc")
    env.Replace(NACL_SDK_LIB=sdk_path + '/nacl64/lib')

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
                  '/llvm-wrapper-sfigcc')
  pnacl_sdk_cxx = (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/llvm-wrapper-sfig++')
  pnacl_sdk_ld =  (pnacl_sdk_root + '/arm-none-linux-gnueabi' +
                  '/llvm-wrapper-bcld')
  # NOTE: XXX_flags start with space for easy concatenation
  pnacl_sdk_cxx_flags = ' -emit-llvm'
  pnacl_sdk_cc_flags = ' -emit-llvm  -std=gnu99'
  pnacl_sdk_cc_native_flags = ' -std=gnu99 -arch %s' % arch
  pnacl_sdk_ld_flags = ' -arch %s' % arch
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
              LINK=pnacl_sdk_ld + pnacl_sdk_ld_flags,
              AR=pnacl_sdk_ar,
              RANLIB=pnacl_sdk_ranlib,
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
    if SCons.Script.ARGUMENTS.get('bitcode'):
      _SetEnvForPnacl(env, env['TARGET_PLATFORM'])
    elif env['TARGET_ARCHITECTURE'] == 'x86':
      _SetEnvForX86Sdk(env, root)
    else:
      print "ERROR: unknown TARGET_ARCHITECTURE: ", env['TARGET_ARCHITECTURE']
      assert 0

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

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
import os
import sys
import SCons.Script
import subprocess
import sync_tgz


NACL_PLATFORM_DIR_MAP = {
    'win32': 'windows',
    'cygwin': 'windows',
    'posix': 'linux',
    'linux': 'linux',
    'linux2': 'linux',
    'darwin': 'mac',
}


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
    platform = NACL_PLATFORM_DIR_MAP[env['PLATFORM']]
    root = os.path.join(env['MAIN_DIR'], 'src', 'third_party', 'nacl_sdk',
                        platform, 'sdk', 'nacl-sdk')
    return root

  elif sdk_mode.startswith('custom:'):
    return os.path.abspath(sdk_mode[len('custom:'):])

  else:
    print 'unknown sdk mode [%s]' % sdk_mode
    assert 0


def DownloadSdk(env):
  """Download and untar the latest sdk.

  Args:
    env: SCons environment in question.
  """

  # Only try to download once.
  try:
    if __builtin__.nacl_sdk_downloaded: return
  except AttributeError:
    __builtin__.nacl_sdk_downloaded = True

  # Get path to extract to.
  target = env.subst('$MAIN_DIR/src/third_party/nacl_sdk/' +
                     NACL_PLATFORM_DIR_MAP[env['PLATFORM']])

  # Set NATIVE_CLIENT_SDK_PLATFORM before substitution.
  env['NATIVE_CLIENT_SDK_PLATFORM'] = NACL_PLATFORM_DIR_MAP[env['PLATFORM']]

  # Allow sdk selection function to be used instead.
  if env.get('NATIVE_CLIENT_SDK_SOURCE'):
    url = env['NATIVE_CLIENT_SDK_SOURCE'](env)
  else:
    # Pick download url.
    url = [
        env.subst(env.get(
            'NATIVE_CLIENT_SDK_URL',
            'http://nativeclient.googlecode.com/svn/data/sdk_tarballs_new/'
            'naclsdk_${NATIVE_CLIENT_SDK_PLATFORM}.tgz')),
        env.get('NATIVE_CLIENT_SDK_USERNAME'),
        env.get('NATIVE_CLIENT_SDK_PASSWORD'),
    ]

  sync_tgz.SyncTgz(url[0], target, url[1], url[2])


def _SetEnvForX86Sdk(env, sdk_path):
  # NOTE: attempts to eliminate this PATH setting and use
  #       absolute path have been futile
  env.PrependENVPath('PATH', sdk_path + '/bin')

  env.Replace(# replace hader and lib paths
              NACL_SDK_INCLUDE=sdk_path + '/nacl/include',
              NACL_SDK_LIB=sdk_path + '/nacl/lib',
              # Replace the normal unix tools with the NaCl ones.
              CC='nacl-gcc',
              CXX='nacl-g++',
              AR='nacl-ar',
              AS='nacl-as',
              # NOTE: use g++ for linking so we can handle c AND c++
              LINK='nacl-g++',
              RANLIB='nacl-ranlib',
              )


def _SetEnvForSdkManually(env):
  env.Replace(# replace header and lib paths
              NACL_SDK_INCLUDE=os.getenv('NACL_SDK_INCLUDE',
                                         'MISSING_SDK_INCLUDE'),
              NACL_SDK_LIB=os.getenv('NACL_SDK_LIB', 'MISSING_SDK_LIB'),
              # Replace the normal unix tools with the NaCl ones.
              CC=os.getenv('NACL_SDK_CC', 'MISSING_SDK_CC'),
              CXX=os.getenv('NACL_SDK_CXX', 'MISSING_SDK_CXX'),
              AR=os.getenv('NACL_SDK_AR', 'MISSING_SDK_AR'),
              AS=os.getenv('NACL_SDK_AS', 'MISSING_SDK_AS'),
              # NOTE: use g++ for linking so we can handle c AND c++
              LINK=os.getenv('NACL_SDK_LINK', 'MISSING_SDK_LINK'),
              RANLIB=os.getenv('NACL_SDK_RANLIB', 'MISSING_SDK_RANLIB'),
              )


def ValidateSdk(env):
  checkables = ['${NACL_SDK_INCLUDE}/stdio.h',
                ]
  for c in checkables:
    if os.path.exists(env.subst(c)):
      continue
    sys.stderr.write(env.subst('''
ERROR: NativeClient SDK does not seem present!,
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

Run again with the --download flag or build the SDK yourself.\n
''' % c))
    sys.exit(-1)


def generate(env):
  """SCons entry point for this tool.

  Args:
    env: The SCons environment in question.

  NOTE: SCons requires the use of this name, which fails lint.
  """

  # make these methods to the top level scons file
  env.AddMethod(DownloadSdk)
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

      COMPONENT_LINKFLAGS=['-Wl,-rpath-link,${COMPONENT_LIBRARY_DIR}'],
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.so', '.a'],
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=[],

      # TODO: This is needed for now to work around unc paths.  Take this out
      # when unc paths are fixed.
      IMPLICIT_COMMAND_DEPENDENCIES=False,

      # TODO: this could be .nexe and then all the .nexe stuff goes away?
      PROGSUFFIX=''  # Force PROGSUFFIX to '' on all platforms.
  )

  # Determine where to get the SDK from.
  if sdk_mode == 'manual':
    _SetEnvForSdkManually(env)
  else:
    _SetEnvForX86Sdk(env, _GetNaclSdkRoot(env, sdk_mode))

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

  env.Append(CCFLAGS=[
      '-fno-stack-protector',
      '-fno-builtin',
      ],
  )

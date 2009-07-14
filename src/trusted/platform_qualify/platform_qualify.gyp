# Copyright 2008, Google Inc.
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

{
  'variables': {
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
  },
  'targets': [
    {
      'target_name': 'platform_qual_lib',
      'type': 'static_library',
      'sources': [
        'nacl_cpuwhitelist.c',
        'nacl_cpuwhitelist.h',
        'nacl_os_qualify.h',
        'vcpuid.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'linux/nacl_os_qualify.c',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'osx/nacl_os_qualify.c',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win/nacl_os_qualify.c',
          ],
        }],
      ],
    },
  ]
}

# TODO:
# # Currently, this is only defined for x86, so only compile if x86.
# if env['TARGET_ARCHITECTURE'] != 'x86':
#   Return()
#vcpuid_env = env.Clone()
#if env.Bit('mac'):
#  vcpuid_env.Append(CCFLAGS = ['-mdynamic-no-pic'])
#if env.Bit('linux'):
#  vcpuid_env.Append(CCFLAGS = ['-msse3'])
#nacl_vcpuid = vcpuid_env.ComponentLibrary('vcpuid', 'vcpuid.c')
#env.Append(LIBS = ['vcpuid', 'platform_qual_lib', 'ncvalidate'])
#
#env.ComponentProgram('platform_qual_test', 'platform_qual_test.c')
#env.ComponentProgram('nacl_cpuwhitelist_test', 'nacl_cpuwhitelist_test.c')
#

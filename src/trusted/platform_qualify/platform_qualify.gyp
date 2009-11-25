# -*- python -*-
# Copyright 2008 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

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
            'linux/sysv_shm_and_mmap.c',
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

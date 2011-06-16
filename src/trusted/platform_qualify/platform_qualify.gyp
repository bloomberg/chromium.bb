# -*- python -*-
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="pqlib"', {
        'sources': [
          'nacl_os_qualify.h',
          'nacl_dep_qualify.h',
        ],
        'conditions': [
          ['OS=="linux"', {
            'sources': [
              'linux/nacl_os_qualify.c',
              'linux/sysv_shm_and_mmap.c',
              'posix/nacl_dep_qualify.c',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'osx/nacl_os_qualify.c',
              'posix/nacl_dep_qualify.c',
            ],
          }],
          ['OS=="win"', {
            'sources': [
              'win/nacl_dep_qualify.c',
              'win/nacl_os_qualify.c',
            ],
          }],
          # x86 common sources
          ['target_arch=="ia32" or target_arch=="x64"', {
            'sources': [
              'arch/x86/nacl_cpuwhitelist.h',
              'arch/x86/nacl_cpuwhitelist.c',
              #'arch/x86/vcpuid.h',
              #'arch/x86/vcpuid.c',
            ],
          }],
          # x86-32 specifics
          ['target_arch=="ia32"', {
            'sources': [
              'arch/x86_32/nacl_dep_qualify_arch.c',
            ],
          }],
          # x86-64 specifics
          ['target_arch=="x64"', {
            'sources': [
              'arch/x86_64/nacl_dep_qualify_arch.c',
            ],
          }],
          # arm specifics
          ['target_arch=="arm"', {
            'sources': [
              'arch/arm/nacl_dep_qualify_arch.c',
            ],
          }],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'platform_qual_lib',
      'type': 'static_library',
      'variables': {
        'target_base': 'pqlib',
      },
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
          # For nacl_cpuid used by nacl_cpuwhitelist.
          '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nacl_cpuid_lib',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'platform_qual_lib64',
          'type': 'static_library',
          'variables': {
            'target_base': 'pqlib',
            'win_target': 'x64',
          },
          'dependencies': [
            # For nacl_cpuid used by nacl_cpuwhitelist.
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nacl_cpuid_lib64',
          ],
        },
      ],
    }],
  ],
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

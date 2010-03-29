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
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nrd_xfer"', {
        'sources': [
          'nacl_desc_base.c',
          'nacl_desc_base.h',
          'nacl_desc_cond.c',
          'nacl_desc_cond.h',
          'nacl_desc_conn_cap.c',
          'nacl_desc_conn_cap.h',
          'nacl_desc_dir.c',
          'nacl_desc_dir.h',
          'nacl_desc_effector.h',
          'nacl_desc_effector_cleanup.c',
          'nacl_desc_effector_cleanup.h',
          'nacl_desc_effector_ldr.c',
          'nacl_desc_effector_ldr.h',
          'nacl_desc_effector_trusted_mem.c',
          'nacl_desc_effector_trusted_mem.h',
          'nacl_desc_imc.c',
          'nacl_desc_imc.h',
          'nacl_desc_imc_bound_desc.c',
          'nacl_desc_imc_bound_desc.h',
          'nacl_desc_imc_shm.c',
          'nacl_desc_imc_shm.h',
          'nacl_desc_invalid.c',
          'nacl_desc_invalid.h',
          'nacl_desc_io.c',
          'nacl_desc_io.h',
          'nacl_desc_mutex.c',
          'nacl_desc_mutex.h',
          'nacl_desc_semaphore.c',
          'nacl_desc_semaphore.h',
          'nacl_desc_sync_socket.c',
          'nacl_desc_sync_socket.h',
          'nacl_desc_wrapper.cc',
          'nacl_desc_wrapper.h',
          'nrd_all_modules.c',
          'nrd_all_modules.h',
          # nrd_xfer_obj = env_no_strict_aliasing.ComponentObject('nrd_xfer.c')
          'nrd_xfer.c',
          'nrd_xfer.h',
          'nrd_xfer_effector.c',
          'nrd_xfer_effector.h',
        ],
        # TODO(bsy,bradnelson): when gyp can do per-file flags, make
        # -fno-strict-aliasing and -Wno-missing-field-initializers
        # apply only to nrd_xfer.c
        'cflags': [
          '-fno-strict-aliasing',
          '-Wno-missing-field-initializers'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-fno-strict-aliasing',
            '-Wno-missing-field-initializers'
          ]
        },
        'conditions': [
          ['OS=="linux"', { 'sources': [
              'linux/nacl_desc.c',
              'linux/nacl_desc_sysv_shm.c',
              'linux/nacl_desc_sysv_shm.h',
          ]}],
          ['OS=="mac"', { 'sources': [
              'linux/nacl_desc.c',
          ]}],
          ['OS=="win"', { 'sources': [
              'win/nacl_desc.c',
          ]}],
        ],
      },
    ]],
  },
  'conditions': [
    ['OS=="linux"', {
      'defines': [
        'XP_UNIX',
      ],
    }],
    ['OS=="mac"', {
      'defines': [
        'XP_MACOSX',
        'XP_UNIX',
        'TARGET_API_MAC_CARBON=1',
        'NO_X11',
        'USE_SYSTEM_CONSOLE',
      ],
    }],
    ['OS=="win"', {
      'defines': [
        'XP_WIN',
        'WIN32',
        '_WINDOWS'
      ],
      'targets': [
        {
          'target_name': 'nrd_xfer64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nrd_xfer',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nrd_xfer',
      'type': 'static_library',
      'variables': {
        'target_base': 'nrd_xfer',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
}

# TODO:
# Currently, this is only defined for x86, so only compile if x86.
#if env['TARGET_ARCHITECTURE'] != 'x86':
#  Return()
#
#if env.Bit('linux'):
#   env_no_strict_aliasing.Append(CCFLAGS = ['-fno-strict-aliasing'])
#
## TODO(robertm): is this still needed?
## Make a copy of debug CRT for now.
## TODO(bradnelson): this really should be avoided if possible.
#crt = env.Replicate('.', '$VC80_DIR/vc/redist/Debug_NonRedist/'
#                    'x86/Microsoft.VC80.DebugCRT')
#crt += env.Replicate('.', '$VC80_DIR/vc/redist/x86/Microsoft.VC80.CRT')
#
#if env['TARGET_PLATFORM'] == 'WINDOWS':
#  env.Append(LIBS = [ 'ws2_32', 'advapi32' ])
#
#env.ComponentProgram('nrd_xfer_test', 'nrd_xfer_test.c',
#                     EXTRA_LIBS=['sel',
#                                 'nrd_xfer',
#                                 'nonnacl_srpc',
#                                 'ncvalidate',
#                                 'google_nacl_imc_c',
#                                 'platform',
#                                 'gio'])
#

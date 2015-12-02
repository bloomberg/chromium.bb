# -*- python -*-
# Copyright 2008 The Native Client Authors. All rights reserved.
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
      ['target_base=="nrd_xfer"', {
        'sources': [
          'nacl_desc_base.c',
          'nacl_desc_base.h',
          'nacl_desc_cond.c',
          'nacl_desc_cond.h',
          'nacl_desc_custom.c',
          'nacl_desc_custom.h',
          'nacl_desc_dir.c',
          'nacl_desc_dir.h',
          'nacl_desc_effector.h',
          'nacl_desc_effector_trusted_mem.c',
          'nacl_desc_effector_trusted_mem.h',
          'nacl_desc_imc.c',
          'nacl_desc_imc.h',
          'nacl_desc_imc_shm.c',
          'nacl_desc_imc_shm.h',
          'nacl_desc_invalid.c',
          'nacl_desc_invalid.h',
          'nacl_desc_io.c',
          'nacl_desc_io.h',
          'nacl_desc_mutex.c',
          'nacl_desc_mutex.h',
          'nacl_desc_null.c',
          'nacl_desc_null.h',
          'nacl_desc_quota.c',
          'nacl_desc_quota.h',
          'nacl_desc_quota_interface.c',
          'nacl_desc_quota_interface.h',
          'nacl_desc_semaphore.c',
          'nacl_desc_semaphore.h',
          'nacl_desc_sync_socket.c',
          'nacl_desc_sync_socket.h',
          'nrd_all_modules.c',
          'nrd_all_modules.h',
          # nrd_xfer_obj = env_no_strict_aliasing.ComponentObject('nrd_xfer.c')
          'nrd_xfer.c',
          'nrd_xfer.h',
          'nrd_xfer_intern.h',
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
          ['os_posix==1', {
            'sources': [
              'posix/nacl_desc.c',
            ],
          }],
          ['OS=="mac"', {
            # Turning -pedantic off is a hack.  Without it, clang
            # complains that macro arguments are empty, which is
            # only permitted in c99 and c++0x modes.  This is true
            # even when DYNAMIC_ANNOTATIONS_ENABLED is 0 (see
            # base/third_party/dynamic_annotations/dynamic_annotations.h).
            # We really should split nacl_desc_wrapper.{cc,h} into its
            # own library to isolate the build warning relaxation to just
            # that one file.  Of course, any dependent of nacl_desc_wrapper.h
            # will also need this.
            'conditions': [
              ['clang==1', {
                'xcode_settings': {
                  'WARNING_CFLAGS!': [
                  '-pedantic',
                ]}
              }]
            ],
            'sources': [
              'osx/nacl_desc_imc_shm_mach.c',
              'osx/nacl_desc_imc_shm_mach.h',
            ],
          }],
          ['OS=="win"', { 'sources': [
              'win/nacl_desc.c',
          ]}],
          ['OS=="win"',
            # String-based bound socket implementation.
            {'sources': [
              'nacl_desc_conn_cap.c',
              'nacl_desc_imc_bound_desc.c',
              'nacl_makeboundsock.c',
             ],
            },
            # FD-based bound socket implementation.
            {
              'sources': [
                'posix/nacl_desc_conn_cap.c',
                'posix/nacl_desc_imc_bound_desc.c',
                'posix/nacl_makeboundsock.c',
              ],
            }
          ],
        ],
      }],
      ['target_base=="desc_wrapper"', {
        'sources': [
          'nacl_desc_wrapper.cc',
          'nacl_desc_wrapper.h',
        ],
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
          ['OS=="mac"', {
            # Turning -pedantic off is a hack.  Without it, clang
            # complains that macro arguments are empty, which is
            # only permitted in c99 and c++0x modes.  This is true
            # even when DYNAMIC_ANNOTATIONS_ENABLED is 0 (see
            # base/third_party/dynamic_annotations/dynamic_annotations.h).
            # We really should split nacl_desc_wrapper.{cc,h} into its
            # own library to isolate the build warning relaxation to just
            # that one file.  Of course, any dependent of nacl_desc_wrapper.h
            # will also need this.
            'conditions': [
              ['clang==1', {
                'xcode_settings': {
                  'WARNING_CFLAGS!': [
                  '-pedantic',
                ]}
              }]
            ]
          }],
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nrd_xfer64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nrd_xfer',
            'win_target': 'x64',
          },
          'dependencies': [
            'desc_wrapper64',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
          ],
        },
        {
          'target_name': 'desc_wrapper64',
          'type': 'static_library',
          'variables': {
             'target_base': 'desc_wrapper',
             'win_target': 'x64',
          },
          'dependencies': [
            # 'nrd_xfer64',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
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
      # TODO(bsy): this is a lie.  desc_wrapper depends on nrd_xfer, not
      # the other way around.  we need this lie until chrome's plugin.gyp
      # is updated to depend on desc_wrapper (which requires Cr to roll
      # DEPS to pick up a new version of NaCl), and then NaCl rolls deps
      # to pick up the new Cr version of plugin.gyp which depends on
      # desc_wrapper (as well as nrd_xfer), after which the bogus
      # dependency of nrd_xfer on desc_wrapper can go away, and the
      # correct dependency added.
      'dependencies': [
        'desc_wrapper',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
      ],
    },
    {
      'target_name': 'desc_wrapper',
      'type': 'static_library',
      'variables': {
        'target_base': 'desc_wrapper',
      },
      'dependencies': [
        # 'nrd_xfer',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
      ],
    },
  ],
}

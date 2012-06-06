#
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'debug_stub.h',
      'debug_stub.cc',
      'event_common.cc',
      'nacl_debug.cc',
      'platform_common.cc',
      'thread_common.cc',
      'transport_common.cc',
    ],
    'conditions': [
      ['OS=="linux" or OS=="mac"', {
        'platform_sources': [
          'posix/debug_stub_posix.cc',
          'posix/mutex_impl.cc',
          'posix/platform_impl.cc',
          'posix/thread_impl.cc',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/debug_stub_win.cc',
          'win/mutex_impl.cc',
          'win/platform_impl.cc',
          'win/thread_impl.cc',
        ],
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-fexceptions',
        ],
        'cflags_cc' : [
          '-frtti',
        ]
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',      # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',            # -frtti
        }
      }],
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-Wno-long-long',
        ],
      }],
      ['target_base=="debug_stub"', {
        'sources': [
          '<@(common_sources)',
          '<@(platform_sources)',
        ],
      }],
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'debug_stub',
      'type': 'static_library',
      'variables': {
        'target_base': 'debug_stub',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'debug_stub64',
          'type': 'static_library',
          'variables': {
            'target_base': 'debug_stub',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp64',
          ],
        },
      ],
    }],
  ],
}


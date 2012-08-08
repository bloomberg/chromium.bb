# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
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
      ['OS=="win"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '1',
            'RuntimeTypeInfo': 'true',
          },
        }
      }],
    ],
  },
  'variables': {
    'gdb_rsp_sources': [
        'abi.cc',
        'abi.h',
        'packet.cc',
        'packet.h',
        'session.cc',
        'session.h',
        'target.cc',
        'target.h',
        'util.cc',
        'util.h',
    ],
    'gdb_test_sources': [
        'abi_test.cc',
        'packet_test.cc',
        'session_test.cc',
        'util_test.cc',
        'test.cc',
        'test.h',
    ],
  },
  'targets': [
    {
      'target_name': 'gdb_rsp',
      'type': 'static_library',
      'sources': [
        '<@(gdb_rsp_sources)',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
    {
      'target_name': 'gdb_rsp_test',
      'type': 'executable',
        'sources': [
          '<@(gdb_test_sources)',
        ],
      'dependencies': [
        'gdb_rsp',
      ]
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gdb_rsp64',
          'type': 'static_library',
          'sources': [
            '<@(gdb_rsp_sources)',
          ],
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
        {
          'target_name': 'gdb_rsp_test64',
          'type': 'executable',
            'sources': [
              '<@(gdb_test_sources)',
            ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'dependencies': [
            'gdb_rsp64',
          ]
        },
      ],
    }],
  ],
}

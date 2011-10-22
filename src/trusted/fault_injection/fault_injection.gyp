# -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nacl_fault_inject"', {
        'sources': [
          'fault_injection.h',
          'fault_injection.c',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-fno-strict-aliasing',
            '-Wno-missing-field-initializers'
          ]
        },
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
          'target_name': 'nacl_fault_inject64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_fault_inject',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_fault_inject',
      'type': 'static_library', # 'dynamic_library', ?!?
      'variables': {
        'target_base': 'nacl_fault_inject',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
}

# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="srt_x86_cmn"', {
        'sources': [
          'nacl_ldt_x86.c',
        ],
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'service_runtime_x86_common',
      'type': 'static_library',
      'variables': {
        'target_base': 'srt_x86_cmn',
      },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'service_runtime_x86_common64',
          'type': 'static_library',
          'variables': {
            'target_base': 'srt_x86_cmn',
            'win_target': 'x64',
          },
        },
      ],
    }],
  ],
}

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="android"', {
        'use_opus_fixed_point%': 1,
      }, {
        'use_opus_fixed_point%': 0,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'opus',
      'type': 'static_library',
      'defines': [
        'OPUS_BUILD',
        'OPUS_EXPORT=',
      ],
      'include_dirs': [
        'src/celt',
        'src/include',
        'src/silk',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
      },
      'includes': ['opus_srcs.gypi', ],
      'conditions': [
        ['OS!="win"', {
          'defines': [
            'HAVE_LRINT',
            'HAVE_LRINTF',
            'VAR_ARRAYS',
          ],
        }, {
          'defines': [
            'USE_ALLOCA',
            'inline=__inline',
          ],
          'msvs_disabled_warnings': [
            4305,  # Disable truncation warning in celt/pitch.c .
            4334,  # Disable 32-bit shift warning in src/opus_encoder.c .
          ],
        }],
        ['use_opus_fixed_point==0', {
          'include_dirs': [
            'src/silk/float',
          ],
          'sources/': [
            ['exclude', '/fixed/[^/]*_FIX.(h|c)$'],
          ],
        }, {
          'defines': [
            'FIXED_POINT',
          ],
          'include_dirs': [
            'src/silk/fixed',
          ],
          'sources/': [
            ['exclude', '/float/[^/]*_FLP.(h|c)$'],
          ],
        }],
      ],
    },  # target opus
    {
      'target_name': 'opus_demo',
      'type': 'executable',
      'dependencies': [
        'opus'
      ],
      'conditions': [
        ['OS == "win"', {
          'defines': [
            'inline=__inline',
          ],
        }],
        ['OS=="android"', {
          'link_settings': {
            'libraries': [
              '-llog',
            ],
          },
        }]
      ],
      'sources': [
        'src/src/opus_demo.c',
      ],
      'include_dirs': [
        'src/celt',
        'src/silk',
      ],
    },  # target opus_demo
  ]
}

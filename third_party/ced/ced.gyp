# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'ced.gypi',
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'ced',
      'type': 'static_library',
      'include_dirs': [
        'src',
      ],
      'sources': [
        '<@(ced_sources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
      'conditions': [
        ['OS == "ios"', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_MSVC',
            ],
          },
          'msvs_disabled_warnings': [4005, 4006, 4018, 4244, 4309, 4800, 4267],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_GCC',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'ced_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
        'ced',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'sources': [
        'src/compact_enc_det/compact_enc_det_fuzz_test.cc',
        'src/compact_enc_det/compact_enc_det_unittest.cc',
        'src/compact_enc_det/detail_head_string.inc',
        'src/util/encodings/encodings_unittest.cc',
      ],
    },
  ],
}

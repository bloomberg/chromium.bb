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
      ['target_base=="jsoncpp"', {
        'include_dirs': [
          '<(DEPTH)/native_client/src/third_party_mod/jsoncpp/include/',
        ],
        'sources': [
          'src/lib_json/json_writer.cpp',
          'src/lib_json/json_value.cpp',
          'src/lib_json/json_reader.cpp',
          'include/json/writer.h',
          'include/json/reader.h',
          'include/json/forwards.h',
          'include/json/value.h',
          'include/json/autolink.h',
          'include/json/json.h',
          'include/json/config.h',
          'include/json/features.h',
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
      },
    ]],
  },
  'conditions': [
    ['OS=="win"', {
      'defines': [
        'XP_WIN',
        'WIN32',
        '_WINDOWS'
      ],
      'targets': [
        {
          'target_name': 'jsoncpp64',
          'type': 'static_library',
          'variables': {
            'target_base': 'jsoncpp',
            'win_target': 'x64',
          },
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'jsoncpp',
      'type': 'static_library',
      'variables': {
        'target_base': 'jsoncpp',
      },
    },
  ],
}

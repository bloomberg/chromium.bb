# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'ced.gypi',
    '../../native_client/build/untrusted.gypi',
  ],
   'target_defaults': {
    'pnacl_compile_flags': [
      '-Wno-c++11-narrowing',
      '-Wno-unused-variable'
    ],
  },
 'targets': [
    {
      'target_name': 'ced_nacl',
      'type': 'none',
      'include_dirs': [
        'src',
      ],
      'variables': {
        'nlib_target': 'libced_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        '<@(ced_sources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
    },
  ],
}



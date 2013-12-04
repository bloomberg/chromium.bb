# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'verbose_libraries_build%': 0,
  },
  'conditions': [
    ['asan==1', {
      'sanitizer_type': 'asan',
    }],
    ['msan==1', {
      'sanitizer_type': 'msan',
    }],
    ['verbose_libraries_build==1', {
      'verbose_libraries_build_flag': '--verbose',
    }, {
      'verbose_libraries_build_flag': '',
    }],
  ],
  'targets': [
    {
      'target_name': 'instrumented_libraries',
      'type': 'none',
      'variables': {
         'prune_self_dependency': 1,
      },
      'dependencies': [
        'libpng12-0',
        'libxau6',
        'libglib2.0-0',
      ],
      'actions': [
        {
          'action_name': 'fix_rpaths',
          'inputs': [
            'fix_rpaths.sh',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/rpaths.fixed.txt',
          ],
          'action': [
            '<(DEPTH)/third_party/instrumented_libraries/fix_rpaths.sh',
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)'
          ],
        },
      ],
    },
    {
      'target_name': 'libpng12-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'target_name': 'libxau6',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'target_name': 'libglib2.0-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
  ],
}

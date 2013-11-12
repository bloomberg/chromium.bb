# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
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
      ],
      'actions': [
        {
          'action_name': 'fix_rpaths',
          'inputs': [
            'fix_rpaths.sh',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/instrumented_libraries/asan/rpaths.fixed.txt',
          ],
          'action': ['./fix_rpaths.sh', '<(PRODUCT_DIR)/instrumented_libraries/asan'],
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
  ],
}

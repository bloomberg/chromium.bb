# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_khronos%': 0,
  },
  'conditions': [
    ['use_system_khronos==0', {
      'targets': [
        {
          'target_name': 'khronos_headers',
          'type': 'none',
          'all_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
        },
      ],
    }, { # use_system_khronos==1
      'targets': [
        {
          'target_name': 'khronos_headers',
          'type': 'none',
          'all_dependent_settings': {
            'include_dirs': [
              '/usr/include',
            ],
          },
        },
      ],
    }],
  ],
}

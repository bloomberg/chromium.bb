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
              '../../gpu',  # Contains GLES2/gl2chromium.h
            ],
          },
        },
      ],
    }, { # use_system_khronos==1
      'targets': [
        {
          'target_name': 'khronos_headers',
          'type': 'none',
          'variables': {
            'headers_root_path': '../..',
            'header_filenames': [
              'GLES2/gl2.h;<GLES2/gl2chromium.h>;',
              'GLES2/gl2ext.h;<GLES2/gl2chromium.h>;<GLES2/gl2extchromium.h>',
            ],
            'shim_generator_additional_args': [
              '--headers-root', '.',
              '--use-include-next',
            ],
          },
          'includes': [
            '../../build/shim_headers.gypi',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '../../gpu',  # Contains GLES2/gl2chromium.h
            ],
          },
        },
      ],
    }],
  ],
}

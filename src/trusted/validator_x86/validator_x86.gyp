# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(bradchen): eliminate need for the warning flag removals below
{
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nccopy"', {
        'sources': [
          'nccopycode.c',
          'nccopycode_stores.S',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
    ],
  },
  # ----------------------------------------------------------------------
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nccopy_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
          },
        }],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'nccopy_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
            'win_target': 'x64',
          },
        }],
    }],
    ['OS!="win" and target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'nccopy_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
          },
        }],
    }],
    [ 'target_arch=="arm"', {
      'targets': []
    }],
  ],
}

# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating x86 code, used
# by both the x86-32 and x86-64 validators.
# Note: Would like to name this file validator_x86.gyp, but that name is
# already used, and on mac's, this is not allowed.
{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nc_x86"', {
        'sources': [
            'error_reporter.c',
            'halt_trim.c',
            'nacl_cpuid.c',
            'ncinstbuffer.c',
            'x86_insts.c',
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
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        { 'target_name': 'ncval_base_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_x86',
          },
        }],
    }],
    ['OS=="win"', {
      'targets': [
        { 'target_name': 'ncval_base_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_x86',
            'win_target': 'x64',
          },
        }],
    }],
    ['OS!="win" and target_arch=="x64"', {
      'targets': [
        { 'target_name': 'ncval_base_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_x86',
          },
        }],
    }],
    [ 'target_arch=="arm"', {
      'targets': []
    }],
  ],
}

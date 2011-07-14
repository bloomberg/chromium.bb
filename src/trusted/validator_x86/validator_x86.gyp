# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(bradchen): eliminate need for the warning flag removals below
{
  'includes': [
    '../../../build/common.gypi',
  ],
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
      ['target_base=="ncopcode_utils"', {
        'sources': ['ncopcode_desc.c'],
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
          ]
        },
      }],
      ['target_base=="ncval_reg_sfi"', {
        # we depend on ncvalidate build to generate the headers
        'sources': [ 'ncvalidate_iter.c',
                     'ncvalidator_registry.c',
                     'nc_cpu_checks.c',
                     'nc_illegal.c',
                     'nc_inst_iter.c',
                     'nc_jumps.c',
                     'nc_opcode_histogram.c',
                     'nc_protect_base.c',
                     'nc_memory_protect.c',
                     'nc_segment.c',
                     'nc_inst_state.c',
                     'nc_inst_trans.c',
                     'ncdis_decode_tables.c',
                     'ncop_exps.c',
                     'ncvalidate_utils.c',
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
          ]
        },
      }],
    ],
  },
  # ----------------------------------------------------------------------
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncopcode_utils_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncopcode_utils',
          },
        }, {
          'target_name': 'nccopy_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
          },
        }, {
          'target_name': 'ncval_reg_sfi_x86_32',
          'type': 'static_library',
          'dependencies': [
             'ncopcode_utils_x86_32',
             'nccopy_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_32'
           ],
          'variables': {
            'target_base': 'ncval_reg_sfi',
          },
        }],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'ncopcode_utils_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncopcode_utils',
            'win_target': 'x64',
          },
        }, {
          'target_name': 'nccopy_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
            'win_target': 'x64',
          },
        }, {
          'target_name': 'ncval_reg_sfi_x86_64',
          'type': 'static_library',
          'dependencies': [
            'ncopcode_utils_x86_64',
            'nccopy_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64'
           ],
          'variables': {
            'target_base': 'ncval_reg_sfi',
            'win_target': 'x64',
          },
        }],
    }],
    ['OS!="win" and target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncopcode_utils_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncopcode_utils',
          },
        }, {
          'target_name': 'nccopy_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nccopy',
          },
        }, {
          'target_name': 'ncval_reg_sfi_x86_64',
          'type': 'static_library',
          'dependencies': [
             'ncopcode_utils_x86_64',
             'nccopy_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64'
           ],
          'variables': {
            'target_base': 'ncval_reg_sfi',
          },
        }],
    }],
    [ 'target_arch=="arm"', {
      'targets': []
    }],
  ],
}

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
      ['target_base=="ncvalidate"', {
        'sources': [
          'halt_trim.c',
          'nacl_cpuid.c',
          'ncdecode.c',
          'error_reporter.c',
          'ncinstbuffer.c',
          'nc_segment.c',
          'nc_inst_iter.c',
          'nc_inst_state.c',
          'nc_inst_trans.c',
          'ncop_exps.c',
          'ncvalidate.c',
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
        # When ncvalidate is a dependency, it needs to be a hard dependency
        # because dependents may rely on ncvalidate to create header files
        # below.
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
      ['target_base=="nacl_cpuid_lib_base"', {
        'sources': ['nacl_cpuid.c'],
      }],
      ['target_base=="ncvalidate_sfi"', {
        # we depend on ncvalidate build to generate the headers
        'sources': [ 'ncvalidate_iter.c',
                     'ncvalidator_registry.c',
                     'nc_opcode_histogram.c',
                     'nc_cpu_checks.c',
                     'nc_illegal.c',
                     'nc_protect_base.c',
                     'nc_memory_protect.c',
                     'ncvalidate_utils.c',
                     'nc_jumps.c',
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
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'nacl_cpuid_lib',
      'type': 'static_library',
      'variables': {
        'target_base': 'nacl_cpuid_lib_base',
      },
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'nchelper',
      'type': 'static_library',
      'sources': [ 'ncfileutil.c' ],
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
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncopcode_utils',
      'type': 'static_library',
      'hard_dependency': 1,
      'variables': {
        'target_base': 'ncopcode_utils',
      },
    },
    {
      'target_name': 'ncvalidate',
      'type': 'static_library',
      'variables': {
        'target_base': 'ncvalidate',
      },
      'dependencies': [
        'ncopcode_utils',
      ],
      'hard_dependency': 1,
    },
    # ----------------------------------------------------------------------
    { 'target_name': 'ncvalidate_sfi',
      'type': 'static_library',
      'dependencies': ['ncvalidate' ],
      'variables': {
        'target_base': 'ncvalidate_sfi',
      },
      'hard_dependency': 1,
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncdis_util',
      'type': 'static_library',
      # we depend on ncvalidate build to generate the headers
      'dependencies': ['ncopcode_utils' ],
      'sources': [ 'ncdis_util.c',
                   'ncdis_segments.c',
                   'nc_read_segment.c',
                   'ncenuminsts.c'
                 ],
      'cflags!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare'
      ],
      'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ]
      },
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'ncvalidate_sfi64',
          'type': 'static_library',
          'dependencies': ['ncvalidate64' ],
          'variables': {
            'target_base': 'ncvalidate_sfi',
            'win_target': 'x64',
          },
          'hard_dependency': 1,
        },
        # ----------------------------------------------------------------------
        {
          'target_name': 'nacl_cpuid_lib64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_cpuid_lib_base',
            'win_target': 'x64',
          },
        },
        # ----------------------------------------------------------------------
        {
          'target_name': 'ncopcode_utils64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncopcode_utils',
            'win_target': 'x64',
          },
        },
        {
          'target_name': 'ncvalidate64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate',
            'win_target': 'x64',
          },
          'dependencies': [
            'ncopcode_utils64',
          ],
          'hard_dependency': 1,
        },
      ],
    }],
  ],
}

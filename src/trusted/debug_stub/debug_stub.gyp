#
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'debug_stub.h',
      'debug_stub.cc',
      'event_common.cc',
      'platform_common.cc',
      'transport_common.cc',
    ],
    'conditions': [
      ['OS=="linux" or OS=="mac"', {
        'platform_sources': [
          'posix/debug_stub_posix.cc',
          'posix/mutex_impl.cc',
          'posix/platform_impl.cc',
          'posix/thread_impl.cc',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/debug_stub_win.cc',
          'win/mutex_impl.cc',
          'win/platform_impl.cc',
          'win/thread_impl.cc',
        ],
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-Wno-long-long',
        ],
      }],
      ['target_base=="debug_stub"', {
        'sources': [
          '<@(common_sources)',
          '<@(platform_sources)',
        ],
      }],
      ['target_base=="debug_stub_test"', {
        'sources': [
          'debug_stub_test.cc',
        ],
      }],
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'debug_stub',
      'type': 'static_library',
      'variables': {
        'target_base': 'debug_stub',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp',
      ],
    },
    # ---------------------------------------------------------------------
    {
      'target_name': 'debug_stub_test',
      'type': 'executable',
      'variables': {
        'target_base': 'debug_stub_test',
      },
      'dependencies': [
        'debug_stub',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'debug_stub64',
          'type': 'static_library',
          'variables': {
            'target_base': 'debug_stub',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp64',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'debug_stub_test64',
          'type': 'executable',
          'variables': {
            'target_base': 'debug_stub_test',
            'win_target': 'x64',
          },
          'dependencies': [
            'debug_stub64',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'run_debug_stub_test',
          'message': 'running test run_imc_tests',
          'type': 'none',
          'dependencies': [
            'debug_stub_test',
            'debug_stub_test64',
          ],
          'actions': [
            {
              'action_name': 'run_debug_stub_test',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(DEPTH)/native_client/tests/debug_stub/test_debug_stub.py',
                '<(PRODUCT_DIR)/debug_stub_test',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/test-output/debug_stub_test.out',
              ],
              'action': [
                '<@(python_exe)',
                '<(DEPTH)/native_client/tests/debug_stub/test_debug_stub.py',
                '<(PRODUCT_DIR)/debug_stub_test',
                '>',
                '<@(_outputs)',
              ],
            },
           ],
           'conditions': [
            ['MSVS_OS_BITS==64', {
              'actions': [
                {
                  'action_name': 'run_debug_stub_test64',
                  'msvs_cygwin_shell': 0,
                  'inputs': [
                    '<(DEPTH)/native_client/tests/debug_stub/test_debug_stub.py',
                    '<(PRODUCT_DIR)/debug_stub_test',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/test-output/debug_stub_test.out',
                  ],
                  'action': [
                    '<@(python_exe)',
                    '<(DEPTH)/native_client/tests/debug_stub/test_debug_stub.py',
                    '<(PRODUCT_DIR)/debug_stub_test64',
                    '>',
                    '<@(_outputs)',
                  ],
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],
}


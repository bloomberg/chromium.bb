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
      'nacl_breakpad.h',
    ],
    'conditions': [
      ['OS=="linux"', {
        'platform_sources': [
          'linux/nacl_breakpad.cc',
        ],
      }],
      ['OS=="mac"', {
        'platform_sources': [
          'osx/nacl_breakpad.cc',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/nacl_breakpad.cc',
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
      ['target_base=="nacl_breakpad"', {
        'sources': [
          '<@(common_sources)',
          '<@(platform_sources)',
        ],
      }],
      ['target_base=="nacl_breakpad_test"', {
        'sources': [
          'nacl_breakpad_test.cc',
        ],
      }],
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'nacl_breakpad',
      'type': 'static_library',
      'variables': {
        'target_base': 'nacl_breakpad',
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_sender',
          ],
        }],
      ],
    },
    # ---------------------------------------------------------------------
    {
      'target_name': 'nacl_breakpad_test',
      'type': 'executable',
      'variables': {
        'target_base': 'nacl_breakpad_test',
      },
      'dependencies': [
        'nacl_breakpad',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'nacl_breakpad64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_breakpad',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler_win64',
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_sender_win64',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'nacl_breakpad_test64',
          'type': 'executable',
          'variables': {
            'target_base': 'nacl_breakpad_test',
            'win_target': 'x64',
          },
          'dependencies': [
            'nacl_breakpad64',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'run_breakpad_test',
          'message': 'running test run_imc_tests',
          'type': 'none',
          'dependencies': [
            'nacl_breakpad_test',
            'nacl_breakpad_test64',
          ],
          'actions': [
            {
              'action_name': 'run_breakpad_test',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(DEPTH)/native_client/tests/breakpad/test_breakpad.py',
                '<(PRODUCT_DIR)/nacl_breakpad_test',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/test-output/nacl_breakpad_test.out',
              ],
              'action': [
                '<@(python_exe)',
                '<(DEPTH)/native_client/tests/breakpad/test_breakpad.py',
                '<(PRODUCT_DIR)/nacl_breakpad_test',
                '>',
                '<@(_outputs)',
              ],
            },
           ],
           'conditions': [
            ['MSVS_OS_BITS==64', {
              'actions': [
                {
                  'action_name': 'run_breakpad_test64',
                  'msvs_cygwin_shell': 0,
                  'inputs': [
                    '<(DEPTH)/native_client/tests/breakpad/test_breakpad.py',
                    '<(PRODUCT_DIR)/nacl_breakpad_test',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/test-output/nacl_breakpad_test.out',
                  ],
                  'action': [
                    '<@(python_exe)',
                    '<(DEPTH)/native_client/tests/breakpad/test_breakpad.py',
                    '<(PRODUCT_DIR)/nacl_breakpad_test64',
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


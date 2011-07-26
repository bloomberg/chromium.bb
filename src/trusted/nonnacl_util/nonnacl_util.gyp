# -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# TODO(sehr): remove need for the warning flag removals below
{
  'includes': [
    # NOTE: this file also defines common dependencies.
    'nonnacl_util.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="sel_ldr_launcher"', {
        'sources': [
          'sel_ldr_launcher.cc',
          'sel_ldr_launcher.h',
          'sel_ldr_launcher_chrome.cc',
        ],
        'cflags!': [
          '-Wextra',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-pedantic',  # import is a gcc extension
            '-Wextra',
          ]
        },
      }],
      ['target_base=="nonnacl_util"', {
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-pedantic',  # import is a gcc extension
          ]
        },
      }],
    ]
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'sel_ldr_launcher',
      'type': 'static_library',
      'variables': {
        'target_base': 'sel_ldr_launcher',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:env_cleanser',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code',
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'nonnacl_util',
      'type': 'static_library',
      'variables': {
        'target_base': 'nonnacl_util',
      },
      'dependencies': [
        'sel_ldr_launcher',
      ],
    },
  ],
  # ----------------------------------------------------------------------
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # --------------------------------------------------------------------
        {
          'target_name': 'sel_ldr_launcher64',
          'type': 'static_library',
          'variables': {
            'target_base': 'sel_ldr_launcher',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:env_cleanser64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code64',
          ],
        },
        # --------------------------------------------------------------------
        {
          'target_name': 'nonnacl_util64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nonnacl_util',
            'win_target': 'x64',
          },
          'dependencies': [
            'sel_ldr_launcher64',
          ],
        },
      ],
    }],
    ['nacl_standalone==0', {
      'targets': [
        # ----------------------------------------------------------------
        {
          'target_name': 'nonnacl_util_chrome',
          'type': 'static_library',
          'dependencies': [
            'sel_ldr_launcher',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:browserhandle',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS=="win" and nacl_standalone==0', {
      'targets': [
        # ----------------------------------------------------------------
        {
          'target_name': 'nonnacl_util_chrome64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            'sel_ldr_launcher64',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:browserhandle64',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# TODO:
# if env.Bit('linux'):
#     env.Append(
#         CCFLAGS=['-fPIC'],
#     )
#if env.Bit('mac'):
#    # there are some issue with compiling ".mm" files
#    env.FilterOut(CCFLAGS=['-pedantic'])
#if env.Bit('windows'):
#    env.Append(
#        CPPDEFINES = ['XP_WIN', 'WIN32', '_WINDOWS'],
#    )

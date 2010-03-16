# Copyright (c) 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'SRPCGEN': '<(DEPTH)/native_client/tools/srpcgen.py',
    'NPRUNTIME_DIR':
    '<(INTERMEDIATE_DIR)/gen/native_client/src/shared/npruntime',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
      'npmodule_specs': [
        'audio.srpc',
        'device2d.srpc',
        'device3d.srpc',
        'npmodule.srpc',
        'npobjectstub.srpc',
      ],
      'npnavigator_specs': [
        'npnavigator.srpc',
        'npobjectstub.srpc',
      ],
      'npupcall_specs': [
        'npupcall.srpc',
      ],
    },
    'target_conditions': [
      ['target_base=="npruntime"', {
        'sources': [
        'nacl_npapi.h',
        'naclnp_util.cc',
        'npbridge.cc',
        'npbridge.h',
        'npcapability.h',
        'npmodule.cc',
        'npmodule.h',
        'npobject_proxy.cc',
        'npobject_proxy.h',
        'npobject_stub.cc',
        'npobject_stub.h',
        'npmodule_rpc_impl.cc',
        'npstub_rpc_impl.cc',
        'npupcall_server.cc',
        'pointer_translations.cc',
        'pointer_translations.h',
        'structure_translations.cc',
        'structure_translations.h',
        ],
      }],
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'XP_MACOSX',
          'XP_UNIX',
          'TARGET_API_MAC_CARBON=1',
          'NO_X11',
          'USE_SYSTEM_CONSOLE',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '2',  # /EHsc
          },
        },
      }],
    ],
    'actions': [
      {
        'action_name': 'npmodule_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(npmodule_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-s',
           'NPModuleRpcs',
           'GEN_NPRUNTIME_NPMODULE_RPC_H_',
           '<@(_outputs)',
           '<@(npmodule_specs)'],

        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<(NPRUNTIME_DIR)/npmodule_rpc.h',
          '<(NPRUNTIME_DIR)/npmodule_rpc_server.cc',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Creating npmodule_rpc.h and npmodule_rpc_server.cc',
      },
      {
        'action_name': 'npnavigator_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(npnavigator_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-c',
           'NPNavigatorRpcs',
           'GEN_NPRUNTIME_NPNAVIGATOR_RPC_H_',
           '<@(_outputs)',
           '<@(npnavigator_specs)'],

        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<(NPRUNTIME_DIR)/npnavigator_rpc.h',
          '<(NPRUNTIME_DIR)/npnavigator_rpc_client.cc',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Creating npnavigator_rpc.h and npnavigator_rpc_client.cc',
      },
      {
        'action_name': 'npupcall_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(npupcall_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-s',
           'NPUpcallRpcs',
           'GEN_NPRUNTIME_NPUPCALL_RPC_H_',
           '<@(_outputs)',
           '<@(npupcall_specs)'],

        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<(NPRUNTIME_DIR)/npupcall_rpc.h',
          '<(NPRUNTIME_DIR)/npupcall_rpc_server.cc',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Creating npupcall_rpc.h and npupcall_rpc_server.cc',
      },
    ],
  },
  'targets': [
    {
      'target_name': 'google_nacl_npruntime',
      'type': 'static_library',
      'variables': {
        'target_base': 'npruntime',
      },
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ]
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'google_nacl_npruntime64',
          'type': 'static_library',
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'variables': {
            'target_base': 'npruntime',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        }
      ],
    }],
  ],
}

#env_no_strict_aliasing = env.Clone()
#if env.Bit('linux'):
#   env_no_strict_aliasing.Append(CCFLAGS = ['-fno-strict-aliasing'])
#

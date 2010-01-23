# Copyright (c) 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
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
  },
  'targets': [
    {
      'target_name': 'google_nacl_npruntime',
      'type': 'static_library',
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
        # TODO env_no_strict_aliasing.ComponentObject('nprpc.cc')
        'nprpc.cc',
        'nprpc.h',
      ],
    }
  ]
}

#env_no_strict_aliasing = env.Clone()
#if env.Bit('linux'):
#   env_no_strict_aliasing.Append(CCFLAGS = ['-fno-strict-aliasing'])
#

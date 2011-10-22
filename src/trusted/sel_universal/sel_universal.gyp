# -*- python -*-
# -*- python -*-
# Copyright (c) 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
      'src_files': ['non_standard_pepper_events.cc',
                    'parsing.cc',
                    'pepper_emu_fileio.cc',
                    'pepper_emu_audio.cc',
                    'pepper_emu_core.cc',
                    'pepper_emu_handler.cc',
                    'pepper_emu_helper.cc',
                    'pepper_emu_postmessage.cc',
                    'pepper_emu_2d.cc',
                    'pepper_emu_3d_stub.cc',
                    'primitives_simple.cc',
                    'replay_handler.cc',
                    'rpc_universal_system.cc',
                    'rpc_universal.cc',
                    'sel_universal.cc',
                    ],
  },
  'targets': [
    {
      'target_name': 'sel_universal',
      'type': 'executable',
      'sources': ['<@(src_files)'],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/ppapi/ppapi.gyp:ppapi_c_nacl',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/reverse_service/reverse_service.gyp:reverse_service',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'sel_universal64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'sources': ['<@(src_files)'],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/ppapi/ppapi.gyp:ppapi_c_nacl',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/reverse_service/reverse_service.gyp:reverse_service64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util64',
          ],
        },
      ],
    }],
  ],
}

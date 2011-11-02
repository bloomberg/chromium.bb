# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../native_client/build/untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl!=1', {
      'targets': [
        {
          'target_name': 'nacl_irt',
          'type': 'none',
          'variables': {
            'nexe_target': 'nacl_irt',
            'out64': '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
            'out32': '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
            'build_glibc': 0,
            'build_newlib': 1,
            'include_dirs': [
              'lib/gl/include',
              '..',
            ],
            'link_flags': [
              '-lirt_browser',
              '-lppruntime',
              '-lsrpc',
              '-limc_syscalls',
              '-lplatform',
              '-lgio',
              '-lm',
            ],
            'sources': [
            ],
          },
          'conditions': [
            ['target_arch=="x64" or target_arch == "ia32"', {
              'variables': {
                'link_flags': [
                  '-Wl,--section-start,.rodata=0x3ef00000',
                  '-Wl,-Ttext-segment=0x0fc00000',
                ],
              },
            }],
          ],
          'dependencies': [
            'src/shared/ppapi_proxy/ppapi_proxy.gyp:ppruntime_lib',
            '../../native_client/src/untrusted/irt/irt.gyp:irt_browser_lib',
            '../../native_client/src/shared/srpc/srpc.gyp:srpc_lib',
            '../../native_client/src/shared/platform/platform.gyp:platform_lib',
            '../../native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
            '../../native_client/src/shared/gio/gio.gyp:gio_lib',
          ],
        },
      ],
    }],
  ],
}

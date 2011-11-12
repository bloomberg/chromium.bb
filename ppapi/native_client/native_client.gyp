# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../native_client/build/common.gypi',
  ],
  'conditions': [
    ['disable_nacl!=1', {
      'targets': [
        {
          'target_name': 'ppapi_lib',
          'type': 'none',
          'dependencies': [
             '../../native_client/src/untrusted/pthread/pthread.gyp:pthread_lib',
             '../../native_client/src/untrusted/irt_stub/irt_stub.gyp:ppapi_stub_lib',
          ],
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
              'files': [
                  '<(DEPTH)/native_client/src/untrusted/irt_stub/libppapi.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
              'files': [
                  '<(DEPTH)/native_client/src/untrusted/irt_stub/libppapi.a',
              ],
            },
          ],
        },
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
            # Link offsets taken from native_client/build/untrusted.gypi
            'link_flags': [
              '-lirt_browser',
              '-lppruntime',
              '-lsrpc',
              '-limc_syscalls',
              '-lplatform',
              '-lgio',
              '-lm',
              '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
              '-Wl,-Ttext-segment=<(NACL_IRT_TEXT_START)',
            ],
            'sources': [
            ],
            'extra_args': [
              '--strip-debug',
            ],
            'extra_deps64': [
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libppruntime.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libirt_browser.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libsrpc.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libplatform.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libimc_syscalls.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libgio.a',
            ],
            'extra_deps32': [
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libppruntime.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libirt_browser.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libsrpc.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libplatform.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libimc_syscalls.a',
              '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libgio.a',
            ],
          },
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

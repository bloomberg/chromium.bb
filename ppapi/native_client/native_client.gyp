# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_lib',
          'type': 'none',
          'dependencies': [
             '../../native_client/src/untrusted/pthread/pthread.gyp:pthread_lib',
             'src/untrusted/irt_stub/irt_stub.gyp:ppapi_stub_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'copies': [
            {
              'destination': '>(tc_include_dir_newlib)/nacl',
              'files': [
                'src/trusted/weak_ref/call_on_main_thread.h',
                'src/shared/ppapi_proxy/ppruntime.h',
              ],
            },

            {
              'destination': '>(tc_lib_dir_pnacl_newlib)',
              'files': [
                'src/untrusted/irt_stub/libppapi.a',
              ],
            },
          ],
          'conditions': [
            ['target_arch!="arm"', {
              'copies': [
                {
                  'destination': '>(tc_include_dir_glibc)/include/nacl',
                  'files': [
                    'src/trusted/weak_ref/call_on_main_thread.h',
                    'src/shared/ppapi_proxy/ppruntime.h',
                  ],
                },
                # Here we copy linker scripts out of the Native Client repo..
                # These are source, not build artifacts.
                {
                  'destination': '>(tc_lib_dir_newlib32)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_newlib64)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_glibc32)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                    'src/untrusted/irt_stub/libppapi.so',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_glibc64)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                    'src/untrusted/irt_stub/libppapi.so',
                  ],
                },
              ]
            }],
            ['target_arch=="arm"', {
              'copies': [
                {
                  'destination': '>(tc_lib_dir_newlib_arm)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
              ]
            }]
          ],
        },
        {
          'target_name': 'nacl_irt',
          'type': 'none',
          'variables': {
            'nexe_target': 'nacl_irt',
            # These out_* fields override the default filenames, which
            # include a "_newlib" suffix and places them in the target
            # directory.
            'out_newlib64': '<(SHARED_INTERMEDIATE_DIR)/nacl_irt_x86_64.nexe',
            'out_newlib32': '<(SHARED_INTERMEDIATE_DIR)/nacl_irt_x86_32.nexe',
            'out_newlib_arm': '<(SHARED_INTERMEDIATE_DIR)/nacl_irt_arm.nexe',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'include_dirs': [
              'lib/gl/include',
              '..',
            ],
            'link_flags': [
              '-Wl,--start-group',
              '-lirt_browser',
              '-lppapi_proxy_untrusted',
              '-lppapi_ipc_untrusted',
              '-lppapi_shared_untrusted',
              '-lgles2_implementation_untrusted',
              '-lgles2_cmd_helper_untrusted',
              '-lgles2_utils_untrusted',
              '-lcommand_buffer_client_untrusted',
              '-lcommand_buffer_common_untrusted',
              '-ltracing_untrusted',
              '-lgpu_ipc_untrusted',
              '-lipc_untrusted',
              '-lbase_untrusted',
              '-lshared_memory_support_untrusted',
              '-lsrpc',
              '-limc_syscalls',
              '-lplatform',
              '-lgio',
              '-Wl,--end-group',
              '-lm',
            ],
            'extra_args': [
              '--strip-all',
            ],
            'conditions': [
              # untrusted.gypi and build_nexe.py currently build
              # both x86-32 and x86-64 whenever target_arch is some
              # flavor of x86.  However, on non-windows platforms
              # we only need one architecture.
              ['OS!="win" and target_arch=="ia32"',
                {
                  'enable_x86_64': 0
                }
              ],
              ['target_arch=="x64"',
                {
                  'enable_x86_32': 0
                }
              ],
              ['target_arch!="arm"', {
                'extra_deps_newlib64': [
                  '>(tc_lib_dir_irt64)/libppapi_proxy_untrusted.a',
                  '>(tc_lib_dir_irt64)/libppapi_ipc_untrusted.a',
                  '>(tc_lib_dir_irt64)/libppapi_shared_untrusted.a',
                  '>(tc_lib_dir_irt64)/libgles2_implementation_untrusted.a',
                  '>(tc_lib_dir_irt64)/libcommand_buffer_client_untrusted.a',
                  '>(tc_lib_dir_irt64)/libcommand_buffer_common_untrusted.a',
                  '>(tc_lib_dir_irt64)/libgpu_ipc_untrusted.a',
                  '>(tc_lib_dir_irt64)/libtracing_untrusted.a',
                  '>(tc_lib_dir_irt64)/libgles2_cmd_helper_untrusted.a',
                  '>(tc_lib_dir_irt64)/libgles2_utils_untrusted.a',
                  '>(tc_lib_dir_irt64)/libipc_untrusted.a',
                  '>(tc_lib_dir_irt64)/libbase_untrusted.a',
                  '>(tc_lib_dir_irt64)/libirt_browser.a',
                  '>(tc_lib_dir_irt64)/libshared_memory_support_untrusted.a',
                  '>(tc_lib_dir_irt64)/libsrpc.a',
                  '>(tc_lib_dir_irt64)/libplatform.a',
                  '>(tc_lib_dir_irt64)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt64)/libgio.a',
                ],
                'extra_deps_newlib32': [
                  '>(tc_lib_dir_irt32)/libppapi_proxy_untrusted.a',
                  '>(tc_lib_dir_irt32)/libppapi_ipc_untrusted.a',
                  '>(tc_lib_dir_irt32)/libppapi_shared_untrusted.a',
                  '>(tc_lib_dir_irt32)/libgles2_implementation_untrusted.a',
                  '>(tc_lib_dir_irt32)/libcommand_buffer_client_untrusted.a',
                  '>(tc_lib_dir_irt32)/libcommand_buffer_common_untrusted.a',
                  '>(tc_lib_dir_irt32)/libgpu_ipc_untrusted.a',
                  '>(tc_lib_dir_irt32)/libtracing_untrusted.a',
                  '>(tc_lib_dir_irt32)/libgles2_cmd_helper_untrusted.a',
                  '>(tc_lib_dir_irt32)/libgles2_utils_untrusted.a',
                  '>(tc_lib_dir_irt32)/libipc_untrusted.a',
                  '>(tc_lib_dir_irt32)/libbase_untrusted.a',
                  '>(tc_lib_dir_irt32)/libirt_browser.a',
                  '>(tc_lib_dir_irt32)/libshared_memory_support_untrusted.a',
                  '>(tc_lib_dir_irt32)/libsrpc.a',
                  '>(tc_lib_dir_irt32)/libplatform.a',
                  '>(tc_lib_dir_irt32)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt32)/libgio.a',
                ],
              }],
              ['target_arch=="arm"', {
                'extra_deps_arm': [
                  '>(tc_lib_dir_irt_arm)/libppapi_proxy_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libppapi_ipc_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libppapi_shared_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_implementation_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libcommand_buffer_client_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libcommand_buffer_common_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libgpu_ipc_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libtracing_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_cmd_helper_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_utils_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libipc_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libbase_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libirt_browser.a',
                  '>(tc_lib_dir_irt_arm)/libshared_memory_support_untrusted.a',
                  '>(tc_lib_dir_irt_arm)/libsrpc.a',
                  '>(tc_lib_dir_irt_arm)/libplatform.a',
                  '>(tc_lib_dir_irt_arm)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt_arm)/libgio.a',
                ],
              }],
            ],
          },
          'dependencies': [
            '../ppapi_proxy_untrusted.gyp:ppapi_proxy_untrusted',
            '../ppapi_ipc_untrusted.gyp:ppapi_ipc_untrusted',
            '../ppapi_shared_untrusted.gyp:ppapi_shared_untrusted',
            '../../gpu/command_buffer/command_buffer_untrusted.gyp:gles2_utils_untrusted',
            '../../gpu/gpu_untrusted.gyp:command_buffer_client_untrusted',
            '../../gpu/gpu_untrusted.gyp:command_buffer_common_untrusted',
            '../../gpu/gpu_untrusted.gyp:gles2_implementation_untrusted',
            '../../gpu/gpu_untrusted.gyp:gles2_cmd_helper_untrusted',
            '../../gpu/gpu_untrusted.gyp:gpu_ipc_untrusted',
            '../../components/tracing_untrusted.gyp:tracing_untrusted',
            '../../ipc/ipc_untrusted.gyp:ipc_untrusted',
            '../../base/base_untrusted.gyp:base_untrusted',
            '../../media/media_untrusted.gyp:shared_memory_support_untrusted',
            '../../native_client/src/untrusted/irt/irt.gyp:irt_browser_lib',
            '../../native_client/src/shared/srpc/srpc.gyp:srpc_lib',
            '../../native_client/src/shared/platform/platform.gyp:platform_lib',
            '../../native_client/src/tools/tls_edit/tls_edit.gyp:tls_edit#host',
            '../../native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
            '../../native_client/src/shared/gio/gio.gyp:gio_lib',
          ],
        },
      ],
    }],
  ],
}

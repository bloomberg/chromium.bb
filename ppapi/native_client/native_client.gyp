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
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include/nacl',
              'files': [
                'src/trusted/weak_ref/call_on_main_thread.h',
                'src/shared/ppapi_proxy/ppruntime.h',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/include/nacl',
              'files': [
                'src/trusted/weak_ref/call_on_main_thread.h',
                'src/shared/ppapi_proxy/ppruntime.h',
              ],
            },
            # Here we copy linker scripts out of the Native Client repository.
            # These are source, not build artifacts.
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
              'files': [
                  'src/untrusted/irt_stub/libppapi.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
              'files': [
                  'src/untrusted/irt_stub/libppapi.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32',
              'files': [
                  'src/untrusted/irt_stub/libppapi.a',
                  'src/untrusted/irt_stub/libppapi.so',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64',
              'files': [
                  'src/untrusted/irt_stub/libppapi.a',
                  'src/untrusted/irt_stub/libppapi.so',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
              'files': [
                'src/untrusted/irt_stub/libppapi.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib',
              'files': [
                'src/untrusted/irt_stub/libppapi.a',
              ],
            },
          ],
        },
        {
          'target_name': 'nacl_irt',
          'type': 'none',
          'variables': {
            'nexe_target': 'nacl_irt',
            # These out_* fields override the default filenames, which
            # include a "_newlib" suffix.
            'out_newlib64': '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
            'out_newlib32': '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
            'out_newlib_arm': '<(PRODUCT_DIR)/nacl_irt_arm.nexe',
            'build_glibc': 0,
            'build_newlib': 1,
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
            # See http://code.google.com/p/nativeclient/issues/detail?id=2691.
            # The PNaCl linker (gold) does not implement the "-Ttext-segment"
            # option.  However, with the linker for x86, the "-Ttext" option
            # does not affect the executable's base address.
            # TODO(olonho): simplify flags handling and avoid duplication
            # with NaCl logic.
            'conditions': [
              ['target_arch!="arm"',
               {
                 'link_flags': [
                   '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
                   '-Wl,-Ttext-segment=<(NACL_IRT_TEXT_START)',
                 ]
               }, { # target_arch == "arm"
                 # TODO(mcgrathr): This knowledge really belongs in
                 # native_client/src/untrusted/irt/irt.gyp instead of here.
                 # But that builds libirt_browser.a as bitcode, so a native
                 # object does not fit happily there.
                 'sources': [
                   '../../native_client/src/untrusted/irt/aeabi_read_tp.S',
                 ],
                 'link_flags': [
                   '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
                   '-Wl,-Ttext=<(NACL_IRT_TEXT_START)',
                   '--pnacl-allow-native',
                   '-arch', 'arm',
                   '-Wt,-mtls-use-call',
                   '-Wl,--pnacl-irt-link',
                 ],
               },
             ],
             # untrusted.gypi and build_nexe.py currently build
             # both x86-32 and x86-64 whenever target_arch is some
             # flavor of x86.  However, on non-windows platforms
             # we only need one architecture.
             ['OS!="win" and target_arch=="ia32"',
               {
                 'enable_x86_64': 0
               }
             ],
             ['OS!="win" and target_arch=="x64"',
               {
                 'enable_x86_32': 0
               }
             ]
            ],
            'sources': [
            ],
            'extra_args': [
              '--strip-debug',
            ],
            # TODO(bradchen): get rid of extra_deps64 and extra_deps32
            # once native_client/build/untrusted.gypi no longer needs them.
            'extra_deps64': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgio.a',
            ],
            'extra_deps32': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgio.a',
            ],
            'extra_deps_newlib64': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libgio.a',
            ],
            'extra_deps_newlib32': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libgio.a',
            ],
            'extra_deps_glibc64': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libgio.a',
            ],
            'extra_deps_glibc32': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libgio.a',
            ],
            'extra_deps_arm': [
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi_proxy_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi_shared_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libgles2_implementation_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libcommand_buffer_client_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libcommand_buffer_common_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libgpu_ipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libtracing_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libgles2_cmd_helper_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libgles2_utils_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libipc_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libbase_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libirt_browser.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libshared_memory_support_untrusted.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libsrpc.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libplatform.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libimc_syscalls.a',
              '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libgio.a',
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
            '../../components/components_tracing_untrusted.gyp:tracing_untrusted',
            '../../ipc/ipc_untrusted.gyp:ipc_untrusted',
            '../../base/base_untrusted.gyp:base_untrusted',
            '../../media/media_untrusted.gyp:shared_memory_support_untrusted',
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

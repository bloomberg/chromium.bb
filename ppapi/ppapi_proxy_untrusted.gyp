# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'ppapi_proxy.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_proxy_untrusted',
          'type': 'none',
          'variables': {
            'ppapi_proxy_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libppapi_proxy_untrusted.a',
            'build_glibc': 0,
            'build_newlib': 1,
            'defines': [
              # Enable threading for the untrusted side of the proxy.
              # TODO(bbudge) remove when this is the default.
              'ENABLE_PEPPER_THREADING',
            ],
          },
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../native_client/tools.gyp:prep_toolchain',
            '../base/base_untrusted.gyp:base_untrusted',
            '../gpu/command_buffer/command_buffer_untrusted.gyp:gles2_utils_untrusted',
            '../gpu/gpu_untrusted.gyp:command_buffer_client_untrusted',
            '../gpu/gpu_untrusted.gyp:command_buffer_common_untrusted',
            '../gpu/gpu_untrusted.gyp:gles2_implementation_untrusted',
            '../gpu/gpu_untrusted.gyp:gles2_cmd_helper_untrusted',
            '../gpu/gpu_untrusted.gyp:gpu_ipc_untrusted',
            '../ipc/ipc_untrusted.gyp:ipc_untrusted',
            '../ppapi/ppapi_shared_untrusted.gyp:ppapi_shared_untrusted',
            '../ppapi/ppapi_ipc_untrusted.gyp:ppapi_ipc_untrusted',
            '../third_party/khronos/khronos.gyp:khronos_headers',
            '../components/components_tracing_untrusted.gyp:tracing_untrusted',
          ],
        },
      ],
    }],
  ],
}

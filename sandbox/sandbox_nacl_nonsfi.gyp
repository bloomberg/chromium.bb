# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'sandbox_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libsandbox_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              # This is the subset of linux build target, needed for
              # nacl_helper_nonsfi's sandbox implementation.
              'linux/services/proc_util.cc',
              'linux/services/thread_helpers.cc',
              'linux/suid/client/setuid_sandbox_client.cc',
              # TODO(hidehiko): Support namespace sandbox and seccomp-bpf
              # sandbox.
            ],
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../native_client/tools.gyp:prep_toolchain',
          ],
        },
      ],
    }],
  ],
}

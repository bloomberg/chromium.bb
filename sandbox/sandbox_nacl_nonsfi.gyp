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
              'linux/bpf_dsl/bpf_dsl.cc',
              'linux/bpf_dsl/codegen.cc',
              'linux/bpf_dsl/dump_bpf.cc',
              'linux/bpf_dsl/policy.cc',
              'linux/bpf_dsl/policy_compiler.cc',
              'linux/bpf_dsl/syscall_set.cc',
              'linux/bpf_dsl/verifier.cc',
              'linux/seccomp-bpf-helpers/sigsys_handlers.cc',
              'linux/seccomp-bpf-helpers/syscall_parameters_restrictions.cc',
              'linux/seccomp-bpf/die.cc',
              'linux/seccomp-bpf/errorcode.cc',
              'linux/seccomp-bpf/sandbox_bpf.cc',
              'linux/seccomp-bpf/syscall.cc',
              'linux/seccomp-bpf/trap.cc',
              'linux/services/credentials.cc',
              'linux/services/namespace_sandbox.cc',
              'linux/services/namespace_utils.cc',
              'linux/services/proc_util.cc',
              'linux/services/resource_limits.cc',
              'linux/services/syscall_wrappers.cc',
              'linux/services/thread_helpers.cc',
              'linux/suid/client/setuid_sandbox_client.cc',
            ],
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
          ],
        },

        {
          'target_name': 'sandbox_linux_test_utils_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libsandbox_linux_test_utils_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'linux/seccomp-bpf/sandbox_bpf_test_runner.cc',
              'linux/tests/sandbox_test_runner.cc',
              'linux/tests/unit_tests.cc',
            ],
          },
          'dependencies': [
            '../testing/gtest_nacl.gyp:gtest_nacl',
          ],
        },
      ],
    }],
  ],
}

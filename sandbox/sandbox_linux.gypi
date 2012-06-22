# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # We have two principal targets: sandbox and sandbox_linux_unittests
    # All other targets are listed as dependencies.
    # FIXME(jln): for historial reasons, sandbox_linux is the setuid sandbox
    # and is its own target.
    {
      'target_name': 'sandbox',
      'type': 'none',
      'conditions': [
        # Only compile in the seccomp mode 1 code for the flag combination
        # where we support it.
        [ 'OS=="linux" and (target_arch=="ia32" or target_arch=="x64") '
          'and toolkit_views==0 and selinux==0', {
          'dependencies': [
            '../seccompsandbox/seccomp.gyp:seccomp_sandbox',
          ],
        }],
        # Similarly, compile seccomp BPF when we support it
        [ 'OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
          'type': 'static_library',
          'dependencies': [
            'seccomp_bpf',
          ],
        }],
      ],
    },
    {
      'target_name': 'sandbox_linux_unittests',
      'type': 'executable',
      'dependencies': [
        'sandbox',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'linux/tests/unit_tests.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        [ 'OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
          'sources': [
            'linux/seccomp-bpf/sandbox_bpf_unittest.cc'
          ],
        }],
      ],
    },
    {
      'target_name': 'seccomp_bpf',
      'type': 'static_library',
      'sources': [
        'linux/seccomp-bpf/sandbox_bpf.cc',
        'linux/seccomp-bpf/sandbox_bpf.h',
        'linux/seccomp-bpf/verifier.cc',
        'linux/seccomp-bpf/verifier.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # The setuid sandbox, for Linux
      'target_name': 'chrome_sandbox',
      'type': 'executable',
      'sources': [
        'linux/suid/linux_util.c',
        'linux/suid/linux_util.h',
        'linux/suid/process_util.h',
        'linux/suid/process_util_linux.c',
        'linux/suid/sandbox.h',
        'linux/suid/sandbox.c',
      ],
      'cflags': [
        # For ULLONG_MAX
        '-std=gnu99',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}

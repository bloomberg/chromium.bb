# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
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
        # This does not include Android.
        [ 'OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
          'type': 'static_library',
          # Compile seccomp mode 2 code on Linux
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
        }],
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

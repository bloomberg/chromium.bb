# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'seccomp_intermediate_dir': '<(INTERMEDIATE_DIR)/seccomp-sandbox',
  },
  'targets': [
    {
      'target_name': 'seccomp_sandbox',
      'type': 'static_library',
      'sources': [
        'access.cc',
        'allocator.cc',
        'allocator.h',
        'clone.cc',
        'exit.cc',
        'debug.cc',
        'getpid.cc',
        'gettid.cc',
        'ioctl.cc',
        'ipc.cc',
        'library.cc',
        'library.h',
        'linux_syscall_support.h',
        'madvise.cc',
        'maps.cc',
        'maps.h',
        'mmap.cc',
        'mprotect.cc',
        'munmap.cc',
        'mutex.h',
        'open.cc',
        'sandbox.cc',
        'sandbox.h',
        'sandbox_impl.h',
        'securemem.cc',
        'securemem.h',
        'sigaction.cc',
        'sigprocmask.cc',
        'socketcall.cc',
        'stat.cc',
        'syscall.cc',
        'syscall.h',
        'syscall_table.c',
        'syscall_table.h',
        'tls.h',
        'trusted_process.cc',
        'trusted_thread.cc',
        'x86_decode.cc',
        'x86_decode.h',
      ],
    },
    {
      'target_name': 'seccomp_tests',
      'type': 'executable',
      'sources': [
        'tests/test_syscalls.cc',
      ],
      'include_dirs': [
         '.',
         '<(seccomp_intermediate_dir)',
      ],
      'dependencies': [
        'seccomp_sandbox',
      ],
      'libraries': [
        '-lpthread',
        '-lutil', # For openpty()
      ],
      'actions': [
        {
          'action_name': 'make_test_list',
          'inputs': [
            'tests/list_tests.py',
            'tests/test_syscalls.cc',
          ],
          'outputs': ['<(seccomp_intermediate_dir)/test-list.h'],
          'action': ['sh', '-c', 'python <(_inputs) > <(_outputs)'],
        },
      ],
    },
    {
      'target_name': 'timestats',
      'type': 'executable',
      'sources': [
        'timestats.cc',
      ],
    },
  ],
}

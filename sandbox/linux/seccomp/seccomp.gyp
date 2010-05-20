# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
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
      'target_name': 'timestats',
      'type': 'executable',
      'sources': [
        'timestats.cc',
      ],
    },
  ],
}

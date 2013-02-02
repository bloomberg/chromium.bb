# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests need to be compiled in the same link unit, so we have to list them
# in a separate .gypi file.
{
  'dependencies': [
    'sandbox',
    '../testing/gtest.gyp:gtest',
  ],
  'include_dirs': [
    '../..',
  ],
  'sources': [
    'tests/main.cc',
    'tests/unit_tests.cc',
    'tests/unit_tests.h',
    'services/broker_process_unittest.cc',
  ],
  'conditions': [
    [ 'compile_suid_client==1', {
      'sources': [
        'suid/client/setuid_sandbox_client_unittest.cc',
      ],
    }],
    [ 'compile_seccomp_bpf==1', {
      'sources': [
        'seccomp-bpf/bpf_tests.h',
        'seccomp-bpf/codegen_unittest.cc',
        'seccomp-bpf/errorcode_unittest.cc',
        'seccomp-bpf/sandbox_bpf_unittest.cc',
        'seccomp-bpf/syscall_iterator_unittest.cc',
        'seccomp-bpf/syscall_unittest.cc',
      ],
    }],
  ],
}

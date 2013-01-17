# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'compile_suid_client': 1,
      }, {
        'compile_suid_client': 0,
      }],
      ['((OS=="linux" or (OS=="android" and target_arch=="arm")) and '
             '(target_arch=="ia32" or target_arch=="x64" or '
              'target_arch=="arm"))', {
        'compile_seccomp_bpf': 1,
      }, {
        'compile_seccomp_bpf': 0,
      }],
    ],
  },
  'target_defaults': {
    'target_conditions': [
      # All linux/ files will automatically be excluded on Android
      # so make sure we re-include them explicitly.
      ['OS == "android"', {
        'sources/': [
          ['include', '^linux/'],
        ],
      }],
    ],
  },
  'targets': [
    # We have two principal targets: sandbox and sandbox_linux_unittests
    # All other targets are listed as dependencies.
    # FIXME(jln): for historial reasons, sandbox_linux is the setuid sandbox
    # and is its own target.
    {
      'target_name': 'sandbox',
      'type': 'none',
      'dependencies': [
        'sandbox_services',
      ],
      'conditions': [
        [ 'compile_suid_client==1', {
          'dependencies': [
            'suid_sandbox_client',
          ],
        }],
        # Only compile in the seccomp mode 1 code for the flag combination
        # where we support it.
        [ 'OS=="linux" and (target_arch=="ia32" or target_arch=="x64") '
          'and toolkit_views==0 and selinux==0', {
          'dependencies': [
            'linux/seccomp-legacy/seccomp.gyp:seccomp_sandbox',
          ],
        }],
        # Similarly, compile seccomp BPF when we support it
        [ 'compile_seccomp_bpf==1', {
          'type': 'static_library',
          'dependencies': [
            'seccomp_bpf',
          ],
        }],
      ],
    },
    {
      'target_name': 'sandbox_linux_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'sandbox',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'tests/main.cc',
        'tests/unit_tests.cc',
        'tests/unit_tests.h',
        'services/broker_process_unittest.cc',
      ],
      'include_dirs': [
        '../..',
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
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ]
        }],
      ],
    },
    {
      'target_name': 'seccomp_bpf',
      'type': 'static_library',
      'sources': [
        'seccomp-bpf/basicblock.cc',
        'seccomp-bpf/basicblock.h',
        'seccomp-bpf/codegen.cc',
        'seccomp-bpf/codegen.h',
        'seccomp-bpf/die.cc',
        'seccomp-bpf/die.h',
        'seccomp-bpf/errorcode.cc',
        'seccomp-bpf/errorcode.h',
        'seccomp-bpf/instruction.h',
        'seccomp-bpf/sandbox_bpf.cc',
        'seccomp-bpf/sandbox_bpf.h',
        'seccomp-bpf/syscall.cc',
        'seccomp-bpf/syscall.h',
        'seccomp-bpf/syscall_iterator.cc',
        'seccomp-bpf/syscall_iterator.h',
        'seccomp-bpf/verifier.cc',
        'seccomp-bpf/verifier.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # The setuid sandbox, for Linux
      'target_name': 'chrome_sandbox',
      'type': 'executable',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/linux_util.c',
        'suid/linux_util.h',
        'suid/process_util.h',
        'suid/process_util_linux.c',
        'suid/sandbox.c',
      ],
      'cflags': [
        # For ULLONG_MAX
        '-std=gnu99',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    { 'target_name': 'sandbox_services',
      'type': 'static_library',
      'sources': [
        'services/broker_process.cc',
        'services/broker_process.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # We make this its own target so that it does not interfere
      # with our tests.
      'target_name': 'libc_urandom_override',
      'type': 'static_library',
      'sources': [
        'services/libc_urandom_override.cc',
        'services/libc_urandom_override.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'suid_sandbox_client',
      'type': 'static_library',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/client/setuid_sandbox_client.cc',
        'suid/client/setuid_sandbox_client.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },

  ],
  'conditions': [
    # Strategy copied from base_unittests_apk in base/base.gyp.
    [ 'OS=="android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
        'target_name': 'sandbox_linux_unittests_apk',
        'type': 'none',
        'dependencies': [
          'sandbox_linux_unittests',
        ],
        'variables': {
          'test_suite_name': 'sandbox_linux_unittests',
          'input_shlib_path':
              '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)sandbox_linux_unittests'
              '<(SHARED_LIB_SUFFIX)',
        },
        'includes': [ '../../build/apk_test.gypi' ],
        }
      ],
    }],
  ],
}

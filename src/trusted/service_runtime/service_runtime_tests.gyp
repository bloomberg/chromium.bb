# -*- python -*-
# Copyright 2010 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="win"', {
        'msvs_cygwin_shell': 0,
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_RTTI': 'YES',              # override -fno-rtti
        },
      }],
      ['target_base=="srt_tests"', {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
        'sources': [
          'unittest_main.cc',
          'sel_memory_unittest.cc',
          # Currently tests the wrong (too low-level) APIs.
          # TODO(bsy,sehr): replace by tests of C API.
          #'nacl_sync_unittest.cc',
          'sel_mem_test.cc',
          'sel_ldr_test.cc',
        ],
        # because of: gtest-typed-test.h:236:46: error:
        # anonymous variadic macros were introduced in C99
        'cflags!': [
          '-pedantic',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-pedantic',
          ],
        },
      }],
    ],
    'cflags_cc!': ['-fno-rtti'],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'service_runtime_tests64',
          'type': 'executable',
          'variables': {
            'target_base': 'srt_tests',
            'win_target': 'x64'
          },
          'dependencies': [
            'service_runtime.gyp:sel64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/third_party_mod/gtest/gtest.gyp:gtest64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
          ],
        },
        # TODO(gregoryd): The tests below should be moved into platform directory.
        # Keeping them here for now for consistency with scons.
        {
          'target_name': 'mmap_test64',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            'service_runtime.gyp:sel64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
          ],
          'variables': {
            'win_target': 'x64'
          },
          'sources': [
            'mmap_test.c',
          ],
        },
      ],
    }],
    ['OS != "mac"', {
      'targets': [
        {
          'target_name': 'nacl_tls_unittest',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
          ],
          'sources': [
            'nacl_tls_unittest.c'
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'service_runtime_tests',
      'type': 'executable',
      'variables': {
        'target_base': 'srt_tests',
      },
      'dependencies': [
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/third_party_mod/gtest/gtest.gyp:gtest',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
      ],
    },
    # TODO(gregoryd): The tests below should be moved into platform directory.
    # Keeping them here for now for consistency with scons.
    {
      'target_name': 'mmap_test',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
      ],
      'sources': [
        'mmap_test.c',
      ],
    },
    {
      'target_name': 'nacl_sync_cond_test',
      'type': 'executable',
      'dependencies': [
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
      'sources': [
        'nacl_sync_cond_test.c',
      ],
    },
   {
    'target_name': 'env_cleanser_test',
    'type': 'executable',
    'dependencies': [
      'service_runtime.gyp:sel',
      '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
    ],
    'sources': [
      'env_cleanser_test.c',
    ],
    },
    {
      'target_name': 'format_string_test',
      'type': 'executable',
      'dependencies': [
        'service_runtime.gyp:sel',
      ],
      'sources': [
        'format_string_test.c',
      ],
    },
    {
      'target_name': 'nacl_check_test',
      'type': 'executable',
      'dependencies': [
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
      ],
      'sources': [
        'nacl_check_test.c',
      ],
    },
    {
      # ignoring condition target_arch == build_arch from scons
      'target_name': 'sel_ldr_thread_death_test',
      'type': 'executable',
      'dependencies': [
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
      ],
      'sources': [
        'sel_ldr_thread_death_test.c'
      ],
    },
  ],

}

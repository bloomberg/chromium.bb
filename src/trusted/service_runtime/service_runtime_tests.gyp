# -*- python -*-
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
    'cflags_cc!': ['-fno-rtti'],
    'conditions': [
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_RTTI': 'YES',              # override -fno-rtti
        },
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'service_runtime_tests',
      'type': 'executable',
      'dependencies': [
        'service_runtime.gyp:sel',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/third_party_mod/gtest/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '<(DEPTH)/testing/gtest/include',
      ],
      'sources': [
        'unittest_main.cc',
        'sel_memory_unittest.cc',
        'nacl_sync_unittest.cc',
        'gio_mem_test.cc',
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
    },
    # TODO(gregoryd): The tests below should be moved into platform directory.
    # Keeping them here for now for consistency with scons.
    {
      'target_name': 'mmap_test',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        'service_runtime.gyp:sel',
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
  ],
}

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
    'common_sources': [
      'nacl_global_secure_random.c',
      'nacl_global_secure_random.h',
      'nacl_host_desc.h',
      'nacl_host_dir.h',
      'nacl_host_desc_common.c',
      'nacl_interruptible_condvar.c',
      'nacl_interruptible_condvar.h',
      'nacl_interruptible_mutex.c',
      'nacl_interruptible_mutex.h',
      'nacl_log.c',
      'nacl_log.h',
      'nacl_secure_random.h',
      'nacl_secure_random_base.h',
      'nacl_secure_random_common.c',
      'nacl_semaphore.h',
      'nacl_sync.cc',
      'nacl_sync.h',
      'nacl_sync_checked.c',
      'nacl_sync_checked.h',
      'nacl_threads.h',
      'nacl_time.h',
      'nacl_timestamp.h',
      'time.cc',
      'time.h',
    ],
    'conditions': [
      ['OS=="linux"', {
        'platform_sources': [
          'linux/condition_variable.cc',
          'linux/lock.cc',
          'linux/nacl_semaphore.c',
          'linux/nacl_threads.c',
          'linux/nacl_host_desc.c',
          'linux/nacl_host_dir.c',
          'linux/nacl_secure_random.c',
          'linux/nacl_time.c',
          'linux/nacl_timestamp.c',
          'linux/time_linux.cc',
        ],
      }],
      ['OS=="mac"', {
        'platform_sources': [
          'linux/condition_variable.cc',
          'linux/lock.cc',
          'osx/nacl_semaphore.c',
          'linux/nacl_threads.c',
          'linux/nacl_time.c',
          'linux/nacl_timestamp.c',
          'linux/time_linux.cc',
          'linux/nacl_host_desc.c',
          'linux/nacl_secure_random.c',
          'linux/nacl_threads.c',
          'osx/nacl_host_dir.c',
          'osx/strnlen_osx.c',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/condition_variable.cc',
          'win/lock.cc',
          'win/lock_impl_win.cc',
          'win/nacl_semaphore.c',
          'win/nacl_threads.c',
          'win/nacl_host_desc.c',
          'win/nacl_host_dir.c',
          'win/nacl_secure_random.c',
          'win/nacl_time.c',
          'win/nacl_timestamp.c',
          'win/port_win.c',
          'win/time_win.cc',
          'win/xlate_system_error.c',
        ],
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
  },
  'targets': [
    {
      'target_name': 'platform',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'cflags': [
            '-Wno-long-long',
          ],
        }],
        ['OS=="linux" and nacl_standalone==1', {
          'link_settings': {
            'libraries': [
              '-lssl',
              '-lcrypto',
              '-lstdc++',
            ],
          },
        }],
        ['OS=="mac" and nacl_standalone==1', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/usr/lib/libcrypto.dylib',
              '$(SDKROOT)/usr/lib/libssl.dylib',
              '$(SDKROOT)/usr/lib/libstdc++.dylib',
            ],
          },
        }],
      ],
      'sources': [
        '<@(common_sources)',
        '<@(platform_sources)',
      ],
      # TODO(sehr,gregoryd): win/nacl_host_dir.c has wide-char warnings
      # and some unicode related compilation option is missing here?
      'msvs_disabled_warnings': [4133],
    },
  ],
}

# TODO:
# if env.Bit('linux'): or env.Bit('mac'):
#    env.FilterOut(CCFLAGS=['-pedantic'])
#if env.Bit('windows'):
#  port_win_test_exe = env.ComponentProgram('port_win_test',
#                                           ['win/port_win_test.c'],
#                                           EXTRA_LIBS=['platform'])
#
#  node = env.CommandTestAgainstGoldenOutput(
#      'port_win_test.out',
#      [port_win_test_exe],
#      size='huge')
#  env.AddNodeToTestSuite(node,
#                         ['large_tests'],
#                         'run_platform_tests')
#

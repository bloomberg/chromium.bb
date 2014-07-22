# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'boringssl_tests.gypi',
  ],
  'targets': [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'includes': [
        'boringssl.gypi',
      ],
      'sources': [
        '<@(boringssl_lib_sources)',
      ],
      'conditions': [
        ['target_arch == "arm"', {
          'sources': [ '<@(boringssl_linux_arm_sources)' ],
        }],
        ['target_arch == "ia32"', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_sources)' ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "android"', {
              'defines': [ 'OPENSSL_NO_ASM' ],
            }],
          ]
        }],
        ['target_arch == "x64"', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_64_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_64_sources)' ],
            }],
            ['OS == "win"', {
              'sources': [ '<@(boringssl_win_x86_64_sources)' ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "win" and OS != "android"', {
              'defines': [ 'OPENSSL_NO_ASM' ],
            }],
          ]
        }],
        ['target_arch != "arm" and target_arch != "ia32" and target_arch != "x64"', {
          'defines': [ 'OPENSSL_NO_ASM' ],
        }],
        ['component == "shared_library"', {
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # no -fvisibility=hidden
          },
          'cflags!': [ '-fvisibility=hidden' ],
          'conditions': [
            ['os_posix == 1 and OS != "mac"', {
              # Avoid link failures on Linux x86-64.
              # See http://rt.openssl.org/Ticket/Display.html?id=2466&user=guest&pass=guest
              'ldflags+': [ '-Wl,-Bsymbolic' ],
            }],
          ],
        }],
      ],
      'include_dirs': [
        'src/include',
        # This is for arm_arch.h, which is needed by some asm files. Since the
        # asm files are generated and kept in a different directory, they
        # cannot use relative paths to find this file.
        'src/crypto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
      },
    },
    {
      'target_name': 'boringssl_unittests',
      'type': 'executable',
      'sources': [
        'boringssl_unittest.cc',
       ],
      'dependencies': [
        '<@(boringssl_test_targets)',
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
      ],
    },
  ],
}

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libcorkscrew',
      'type': 'static_library',
      'sources': [
        'src/libcorkscrew/backtrace-helper.c',
        'src/libcorkscrew/ptrace.c',
        'src/libcorkscrew/symbol_table.c',
        'src/libcorkscrew/test.cpp',
        'src/libcorkscrew/map_info.c',
        'src/libcorkscrew/backtrace.c',
        'src/libcorkscrew/demangle.c',
        'src/gcc-demangle/cp-demangle.c',
      ],
      'conditions': [
        ['target_arch == "arm"', {
          'sources': [
            'src/libcorkscrew/arch-arm/ptrace-arm.c',
            'src/libcorkscrew/arch-arm/backtrace-arm.c',
          ],
        }],
        ['target_arch == "ia32"', {
          'sources': [
            'src/libcorkscrew/arch-x86/backtrace-x86.c',
            'src/libcorkscrew/arch-x86/dwarf.h',
            'src/libcorkscrew/arch-x86/ptrace-x86.c',
          ],
        }],
        ['target_arch == "mipsel"', {
          'sources': [
            'src/libcorkscrew/arch-mips/backtrace-mips.c',
            'src/libcorkscrew/arch-mips/ptrace-mips.c',
          ],
        }],
      ],
      'defines': [
        'HAVE_STRING_H',
        'HAVE_STDLIB_H',
        'IN_GLIBCPP_V3',
        'CORKSCREW_HAVE_ARCH',
      ],
      'cflags': [
        '-std=c99',
        '-Werror',
      ],
      'include_dirs': [
        'overrides',
        'src/libcorkscrew-include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/libcorkscrew-include',
        ],
      },
    },
  ],
}

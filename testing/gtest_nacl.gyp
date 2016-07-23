# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'gtest.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'gtest_nacl',
          'type': 'none',
          'variables': {
            'nlib_target': 'libgtest_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 1,
            'build_nonsfi_helper': 1,
          },
          'sources': [
            '<@(gtest_sources)',
          ],
          'include_dirs': [
            'gtest',
            'gtest/include',
          ],
          'defines': [
            # In order to allow regex matches in gtest to be shared between
            # Windows and other systems, we tell gtest to always use it's
            # internal engine.
            'GTEST_HAS_POSIX_RE=0',
          ],
          'all_dependent_settings': {
            'defines': [
              'GTEST_HAS_POSIX_RE=0',
            ],
            'link_flags': [
              '-lgtest_nacl',
            ],
          },
          'direct_dependent_settings': {
            'defines': [
              'UNIT_TEST',
            ],
            'include_dirs': [
              'gtest/include',  # So that gtest headers can find themselves.
            ],
          },
        },
        {
          'target_name': 'gtest_main_nacl',
          'type': 'none',
          'variables': {
            'nlib_target': 'libgtest_main_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 1,
            'build_nonsfi_helper': 1,
          },
          'dependencies': [
            'gtest_nacl',
          ],
          'sources': [
            'gtest/src/gtest_main.cc',
          ],
          'all_dependent_settings': {
            'link_flags': [
              '-lgtest_main_nacl',
            ],
          },
        },
      ],
    }],
  ],
}

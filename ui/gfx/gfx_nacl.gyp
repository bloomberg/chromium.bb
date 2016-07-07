# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'variables': {
    'chromium_code': 1,
    # nacl_win64_target is for building the trusted Win64 NaCl broker.
    'nacl_win64_target': 0,
  },
  'includes': [
    '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'gfx_geometry_nacl',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libgfx_geometry_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'dependencies': [
            '../../base/base_nacl.gyp:base_nacl',
            '../../base/base_nacl.gyp:base_nacl_nonsfi',
          ],
          'includes': [
            'gfx_geometry.gypi',
          ],
        },
      ],
    }],
  ],
}

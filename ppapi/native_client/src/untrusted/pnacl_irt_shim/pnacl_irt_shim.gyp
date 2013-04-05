# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'pnacl_irt_shim',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpnacl_irt_shim.a',
        'out_newlib_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-arm/libpnacl_irt_shim.a',
        'out_newlib32': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32/libpnacl_irt_shim.a',
        'out_newlib64': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64/libpnacl_irt_shim.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'force_arm_pnacl': 1,
        'sources': [
          'pnacl_shim.c',
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'include_dirs': [
          '../../../..',
        ],
        'extra_args': [
          '--strip-debug',
        ],
        'conditions': [
          ['target_arch=="arm"', {
            'compile_flags': [
              '--pnacl-allow-translate',
              '--pnacl-allow-native',
              '-arch', 'arm',
            ],
          }],
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}

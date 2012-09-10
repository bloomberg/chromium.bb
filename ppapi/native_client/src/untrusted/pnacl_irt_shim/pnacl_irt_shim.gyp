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
        'nlib_target': 'pnacl_irt_shim.a',
        'out_newlib64': '<(PRODUCT_DIR)/libpnacl_irt_shim.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'enable_x86_32': 0,
        'enable_arm': 0,
        'sources': [
          'pnacl_shim.c',
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'include_dirs': [
          '../../../..',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}

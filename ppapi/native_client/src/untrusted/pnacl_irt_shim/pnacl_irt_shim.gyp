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
        'out_pnacl_newlib_arm': '>(tc_lib_dir_pnacl_translate)/lib-arm/>(nlib_target)',
        'out_pnacl_newlib_x86_32': '>(tc_lib_dir_pnacl_translate)/lib-x86-32/>(nlib_target)',
        'out_pnacl_newlib_x86_64': '>(tc_lib_dir_pnacl_translate)/lib-x86-64/>(nlib_target)',
        'out_pnacl_newlib_mips': '>(tc_lib_dir_pnacl_translate)/lib-mips32/>(nlib_target)',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'pnacl_native_biased': 1,
        'enable_x86_32': 1,
        'enable_x86_64': 1,
        'enable_arm': 1,
        'enable_mips': 1,
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
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}

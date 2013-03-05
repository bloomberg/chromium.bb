# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is used via 'includes' by an irt_test.gyp both here and in
# chromium/src/ppapi/native_client.  The including file must first include
# build/common.gypi to get 'python_exe' set.  The including file must first
# set the variable 'irt_test_nexe' (e.g. to 'irt_core'); setting that to ''
# (the empty string) instructs this file to run the test on the
# nacl_irt_*.nexe file instead of a named one.  The including file must
# also set the variable 'irt_test_dep' to the 'foo.gyp:bar' Gyp target that
# builds the nexe to be tested.

{
  'conditions': [
    ['irt_test_nexe==""', {
      'variables': {
        'check_tls_nexe': '>(nacl_irt_name).nexe',
        'check_tls_target': 'nacl_irt',
      },
    }, {
      'variables': {
        'check_tls_nexe': '<(irt_test_nexe)_>(nexe_suffix).nexe',
        'check_tls_target': '<(irt_test_nexe)',
      },
    }]
  ],
  'targets': [
    {
      'target_name': '<(check_tls_target)_tls_check',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:untar_toolchains',
        '<(irt_test_dep)',
      ],
      'actions': [
        {
          'action_name': 'run_tls_check',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<(PRODUCT_DIR)/<(check_tls_nexe)',
          ],
          'outputs': ['<(PRODUCT_DIR)/<(_target_name).out'],
          'message': 'Sanity-checking TLS usage in <(check_tls_nexe)',
          'conditions': [
            ['target_arch=="arm"', {
              'variables': {
                'nacl_objdump':
                  '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/host_x86_32/bin/arm-pc-nacl-objdump',
              },
            }, {
              # target_arch!="arm", so it's x86.
              'variables': {
                'nacl_objdump':
                  '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/bin/x86_64-nacl-objdump',
              },
            }],
            ['target_arch=="x64"', {
              'variables': {
                'check_tls_arch': 'x86-64',
                'nexe_suffix': 'newlib_x64',
                'nacl_irt_name': 'nacl_irt_x86_64',
              },
            }],
            ['target_arch=="ia32"', {
              'variables': {
                'check_tls_arch': 'x86-32',
                'nexe_suffix': 'newlib_x32',
                'nacl_irt_name': 'nacl_irt_x86_32',
              },
            }],
            ['target_arch=="arm"', {
              'variables': {
                'check_tls_arch': 'arm',
                'nexe_suffix': 'newlib_arm',
                'nacl_irt_name': 'nacl_irt_arm',
              },
            }],
          ],
          'action': ['>(python_exe)',
                     '<(DEPTH)/native_client/src/untrusted/irt/check_tls.py',
                     '<(check_tls_arch)', '<(nacl_objdump)',
                     '<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
  ],
}

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # re: untrusted.gypi -- Though this doesn't really use build_nexe.py or
  # anything, it depends on untrusted nexes from the toolchain and for the shim.
  'includes': [
    '../../../../../build/common_untrusted.gypi',
  ],
  'targets': [
  {
    'target_name': 'pnacl_support_extension',
    'type': 'none',
    'conditions': [
      ['disable_nacl==0 and disable_nacl_untrusted==0', {
        'dependencies': [
          '../../../../../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:pnacl_irt_shim',
          '../../../../../native_client/tools.gyp:prep_toolchain',
        ],
        'sources': [
          'pnacl_info_template.json',
        ],
        # We could use 'copies', but we want to rename the files
        # in a white-listed way first.  Thus use a script.
        'actions': [
          {
            'action_name': 'generate_pnacl_support_extension',
            'inputs': [
              'pnacl_component_crx_gen.py',
              # A stamp file representing the contents of pnacl_translator.
              '<(DEPTH)/native_client/toolchain/pnacl_translator/SOURCE_SHA1',
            ],
            'conditions': [
                # On windows we need both ia32 and x64.
                ['OS=="win"', {
                    'outputs': [
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbegin_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbeginS_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_ld_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libcrt_platform_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_eh_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libpnacl_irt_shim_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_llc_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbegin_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbeginS_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_ld_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libcrt_platform_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_eh_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libpnacl_irt_shim_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_llc_nexe',
                    ],
                    'inputs': [
                      '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32/libpnacl_irt_shim.a',
                      '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64/libpnacl_irt_shim.a',
                    ],
                    'variables': {
                      'lib_overrides': [
                        # Use the two freshly generated shims.
                        '--lib_override=ia32,<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32/libpnacl_irt_shim.a',
                        '--lib_override=x64,<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64/libpnacl_irt_shim.a',
                      ],
                    },
                }],
                # Non-windows installers only need the matching architecture.
                ['OS!="win"', {
                   'conditions': [
                      ['target_arch=="arm"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_crtbeginS_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_llc_nexe',
                        ],
                        'inputs': [
                          '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-arm/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=arm,<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-arm/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="ia32"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbeginS_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_llc_nexe',
                        ],
                        'inputs': [
                          '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=ia32,<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="x64"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbeginS_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_llc_nexe',
                        ],
                        'inputs': [
                          '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=x64,<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                  ],
               }],
            ],
            'action': [
              'python', 'pnacl_component_crx_gen.py',
              '--dest=<(PRODUCT_DIR)/pnacl',
              '<@(lib_overrides)',
              '--installer_only=<(target_arch)',
              # ABI Version Number.
              '0.0.0.1',
            ],
          },
        ],
      }],
    ],
  }],
}

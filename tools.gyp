# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'variables': {
    'disable_glibc%': 0,
    'disable_newlib%': 0,
    'disable_pnacl%': 0,
    'disable_arm%': 0,
    'disable_glibc_untar%': 0,
    'disable_newlib_untar%': 0,
    'disable_arm_untar%': 0,
    'disable_pnacl_untar%': 0,
    'nacl_x86_newlib_toolchain%': "",
    'nacl_x86_newlib_stamp%': "nacl_x86_newlib.json",
    'nacl_arm_newlib_toolchain%': "",
    'nacl_arm_newlib_stamp%': "nacl_arm_newlib.json",
    'nacl_x86_glibc_toolchain%': "",
    'nacl_x86_glibc_stamp%': "nacl_x86_glibc.json",
    'pnacl_newlib_toolchain%': "",
    'pnacl_newlib_stamp%': "pnacl_newlib.json",
    'conditions': [
      ['OS=="android"', {
        'TOOLCHAIN_OS': 'linux',
      }, {
        'TOOLCHAIN_OS': '<(OS)',
      }],
    ]
  },
  'targets' : [
    {
      'target_name': 'prep_toolchain',
      'type': 'none',
      'dependencies': [
        'untar_toolchains',
        'prep_nacl_sdk',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            'crt_init_32',
            'crt_fini_32',
            'crt_init_64',
            'crt_fini_64',
          ],
        }],
        ['target_arch=="arm"', {
          'dependencies': [
            'crt_init_arm',
            'crt_fini_arm',
          ]
        }],
      ],
    },
    {
      'target_name': 'untar_toolchains',
      'type': 'none',
      'conditions': [
        ['nacl_x86_newlib_toolchain=="" and disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Untar x86 newlib toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar x86 newlib toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/package_version/package_version.py',
                 '<(DEPTH)/native_client/toolchain/.tars/<(TOOLCHAIN_OS)_x86/nacl_x86_newlib.json',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_newlib/nacl_x86_newlib.json'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '--quiet',
                '--packages', 'nacl_x86_newlib',
                '--tar-dir', '<(DEPTH)/native_client/toolchain/.tars',
                '--dest-dir', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                'extract',
              ],
            },
          ],
        }],
        ['nacl_x86_newlib_toolchain!="" and disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Copy x86 newlib toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Copy x86 newlib toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/copy_directory.py',
                 '<(nacl_x86_newlib_toolchain)/<(nacl_x86_newlib_stamp)',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_newlib/<(nacl_x86_newlib_stamp)'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/copy_directory.py',
                '--quiet',
                '--stamp-arg', 'nacl_x86_newlib_stamp',
                '--stamp-file', '<(nacl_x86_newlib_stamp)',
                '<(nacl_x86_newlib_toolchain)',
                '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_newlib',
              ],
            },
          ],
        }],
        ['nacl_x86_glibc_toolchain=="" and disable_glibc==0 and disable_glibc_untar==0', {
          'actions': [
            {
              'action_name': 'Untar x86 glibc toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar x86 glibc toolchain',
              'inputs': [
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '<(DEPTH)/native_client/toolchain/.tars/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc.json',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '--quiet',
                '--packages', 'nacl_x86_glibc',
                '--tar-dir', '<(DEPTH)/native_client/toolchain/.tars',
                '--dest-dir', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                'extract',
              ],
            },
          ],
        }],
        ['nacl_x86_glibc_toolchain!="" and disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Copy x86 glibc toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Copy x86 glibc toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/copy_directory.py',
                 '<(nacl_x86_glibc_toolchain)/<(nacl_x86_glibc_stamp)',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/<(nacl_x86_glibc_stamp)'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/copy_directory.py',
                '--quiet',
                '--stamp-arg', 'nacl_x86_glibc_stamp',
                '--stamp-file', '<(nacl_x86_glibc_stamp)',
                '<(nacl_x86_glibc_toolchain)',
                '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc',
              ],
            },
          ],
        }],
        ['pnacl_newlib_toolchain=="" and disable_pnacl==0 and disable_pnacl_untar==0', {
          'actions': [
            {
              'action_name': 'Untar pnacl toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar pnacl toolchain',
              'inputs': [
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '<(DEPTH)/native_client/toolchain/.tars/<(TOOLCHAIN_OS)_x86/pnacl_newlib.json',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '--quiet',
                '--packages', 'pnacl_newlib',
                '--tar-dir', '<(DEPTH)/native_client/toolchain/.tars',
                '--dest-dir', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                'extract',
              ],
            },
          ],
        }],
        ['pnacl_newlib_toolchain!="" and disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Copy pnacl toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Copy pnacl toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/copy_directory.py',
                 '<(pnacl_newlib_toolchain)/<(pnacl_newlib_stamp)',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/pnacl_newlib/<(pnacl_newlib_stamp)'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/copy_directory.py',
                '--quiet',
                '--stamp-arg', 'pnacl_newlib_stamp',
                '--stamp-file', '<(pnacl_newlib_stamp)',
                '<(pnacl_newlib_toolchain)',
                '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/pnacl_newlib',
              ],
            },
          ],
        }],
        ['nacl_arm_newlib_toolchain=="" and target_arch=="arm" and disable_arm==0 and disable_arm_untar==0', {
          'actions': [
            {
              'action_name': 'Untar arm toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar arm toolchain',
              'inputs': [
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '<(DEPTH)/native_client/toolchain/.tars/<(TOOLCHAIN_OS)_x86/nacl_arm_newlib.json',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_arm_newlib/nacl_arm_newlib.json'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/package_version/package_version.py',
                '--quiet',
                '--packages', 'nacl_arm_newlib',
                '--tar-dir', '<(DEPTH)/native_client/toolchain/.tars',
                '--dest-dir', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                'extract',
              ],
            },
          ],
        }],
        ['nacl_arm_newlib_toolchain!="" and disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Copy arm toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Copy arm toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/copy_directory.py',
                 '<(nacl_arm_newlib_toolchain)/<(nacl_arm_newlib_stamp)',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_arm_newlib/<(nacl_arm_newlib_stamp)'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/copy_directory.py',
                '--quiet',
                '--stamp-arg', 'nacl_arm_newlib_stamp',
                '--stamp-file', '<(nacl_arm_newlib_stamp)',
                '<(nacl_arm_newlib_toolchain)',
                '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_arm_newlib',
              ],
            },
          ],
        }],
      ]
    },
    {
      'target_name': 'prep_nacl_sdk',
      'type': 'none',
      'dependencies': [
        'untar_toolchains',
      ],
      'variables': {
        'newlib_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_newlib',
        'glibc_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc',
        'pnacl_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/pnacl_newlib',
        'arm_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(TOOLCHAIN_OS)_x86/nacl_arm_newlib',
      },
      'conditions': [
        ['disable_newlib==0', {
          'actions': [
            {
              'action_name': 'Prep x86 newlib toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Prep x86 newlib toolchain',
              'inputs': [
                 '<(newlib_dir)/<(nacl_x86_newlib_stamp)',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool x86_newlib)',
              ],
              'outputs': ['<(newlib_dir)/stamp.prep'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/prep_nacl_sdk.py',
                '--tool', 'x86_newlib',
                '--path', '<(newlib_dir)',
              ],
            },
          ]
        }],
        ['disable_glibc==0', {
          'actions': [
            {
              'action_name': 'Prep x86 glibc toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Prep x86 glibc toolchain',
              'inputs': [
                 '<(glibc_dir)/<(nacl_x86_glibc_stamp)',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool x86_glibc)',
              ],
              'outputs': ['<(glibc_dir)/stamp.prep'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/prep_nacl_sdk.py',
                '--tool', 'x86_glibc',
                '--path', '<(glibc_dir)',
              ],
            },
          ]
        }],
        ['target_arch=="arm" and disable_arm==0', {
          'actions': [
            {
              'action_name': 'Prep arm toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Prep arm toolchain',
              'inputs': [
                 '<(arm_dir)/<(nacl_arm_newlib_stamp)',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool arm_newlib)',
              ],
              'outputs': ['<(arm_dir)/stamp.prep'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/prep_nacl_sdk.py',
                '--tool', 'arm_newlib',
                '--path', '<(arm_dir)',
              ],
            },
          ]
        }],
        ['disable_pnacl==0', {
          'actions': [
            {
              'action_name': 'Prep pnacl toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Prep pnacl toolchain',
              'inputs': [
                 '<(pnacl_dir)/<(pnacl_newlib_stamp)',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool pnacl)',
              ],
              'outputs': ['<(pnacl_dir)/stamp.prep'],
              'action': [
                'python',
                '<(DEPTH)/native_client/build/prep_nacl_sdk.py',
                '--tool', 'pnacl',
                '--path', '<(pnacl_dir)',
              ],
            },
          ]
        }],
      ]
    },
  ],
  # The crt_init_* targets only need to be built for non-pnacl newlib-based
  # toolchains (and for the IRT if the IRT is built with such a toolchain).
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets' : [
        {
          'target_name': 'crt_init_64',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crti.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 0,
            'enable_x86_32': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_64'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crti_x86_64.S',
          ]
        },
        {
          'target_name': 'crt_fini_64',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crtn.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 0,
            'enable_x86_32': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_64'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crtn_x86_64.S'
          ],
        }
      ],
    }],
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets' : [
        {
          'target_name': 'crt_init_32',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crti.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 0,
            'enable_x86_64': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_32'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crti_x86_32.S',
          ],
        },
        {
          'target_name': 'crt_fini_32',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crtn.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 0,
            'enable_x86_64': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_32'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crtn_x86_32.S'
          ],
        }
      ],
    }],
    ['target_arch=="arm"', {
      'targets' : [
        {
          'target_name': 'crt_init_arm',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crti.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 1,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_arm'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crti_arm.S',
          ],
        },
        {
          'target_name': 'crt_fini_arm',
          'type': 'none',
          'dependencies': [
            'untar_toolchains',
            'prep_nacl_sdk'
          ],
          'variables': {
            'nlib_target': 'crtn.o',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'build_irt': 1,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_arm'
            ],
          },
          'sources': [
            'src/untrusted/stubs/crtn_arm.S'
          ],
        }
      ],
    }],
  ],
}

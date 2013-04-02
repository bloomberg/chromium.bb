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
      'variables': {
        'newlib_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib',
        'glibc_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc',
        'pnacl_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl',
        'arm_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_arm_newlib',
      },
      'conditions': [
        ['disable_newlib==0 and disable_newlib_untar==0', {
          'actions': [
            {
              'action_name': 'Untar x86 newlib toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar x86 newlib toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/naclsdk_<(OS)_x86.tgz',
              ],
              'outputs': ['>(newlib_dir)/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'x86_newlib',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/naclsdk_<(OS)_x86.tgz',
              ],
            },
          ]
        }],
        ['disable_glibc==0 and disable_glibc_untar==0', {
          'actions': [
            {
              'action_name': 'Untar x86 glibc toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar x86 glibc toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/toolchain_<(OS)_x86.tar.bz2',
              ],
              'outputs': ['>(glibc_dir)/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'x86_glibc',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/toolchain_<(OS)_x86.tar.bz2',
              ],
            },
          ]
        }],
        ['disable_pnacl==0 and disable_pnacl_untar==0', {
          'actions': [
            {
              'action_name': 'Untar pnacl toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar pnacl toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/naclsdk_pnacl_<(OS)_x86.tgz',
              ],
              'outputs': ['>(pnacl_dir)/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'x86_pnacl',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/naclsdk_pnacl_<(OS)_x86.tgz',
              ],
            },
          ]
        }],
        ['target_arch=="arm" and disable_arm==0 and disable_arm_untar==0', {
          'actions': [
            {
              'action_name': 'Untar arm toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Untar arm toolchain',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/gcc_arm.tgz',
                 '<(DEPTH)/native_client/toolchain/.tars/binutils_arm.tgz',
                 '<(DEPTH)/native_client/toolchain/.tars/newlib_arm.tgz',
                 '<(DEPTH)/native_client/toolchain/.tars/gcc_libs_arm.tgz',
              ],
              'outputs': ['>(arm_dir)/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'arm_newlib',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/gcc_arm.tgz',
                '<(DEPTH)/native_client/toolchain/.tars/binutils_arm.tgz',
                '<(DEPTH)/native_client/toolchain/.tars/newlib_arm.tgz',
                '<(DEPTH)/native_client/toolchain/.tars/gcc_libs_arm.tgz',
              ],
            },
          ]
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
        'newlib_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib',
        'glibc_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc',
        'arm_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_arm_newlib',
        'pnacl_dir': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl',
      },
      'conditions': [
        ['disable_newlib==0', {
          'actions': [
            {
              'action_name': 'Prep x86 newlib toolchain',
              'msvs_cygwin_shell': 0,
              'description': 'Prep x86 newlib toolchain',
              'inputs': [
                 '<(newlib_dir)/stamp.untar',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool x86_newlib)',
              ],
              'outputs': ['<(newlib_dir)/stamp.prep'],
              'action': [
                '>(python_exe)',
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
                 '<(glibc_dir)/stamp.untar',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool x86_glibc)',
              ],
              'outputs': ['<(glibc_dir)/stamp.prep'],
              'action': [
                '>(python_exe)',
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
                 '<(arm_dir)/stamp.untar',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool arm_newlib)',
              ],
              'outputs': ['<(arm_dir)/stamp.prep'],
              'action': [
                '>(python_exe)',
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
                 '<(pnacl_dir)/stamp.untar',
                 '>!@pymod_do_main(prep_nacl_sdk --inputs --tool x86_pnacl)',
              ],
              'outputs': ['<(pnacl_dir)/stamp.prep'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/prep_nacl_sdk.py',
                '--tool', 'x86_pnacl',
                '--path', '<(pnacl_dir)',
              ],
            },
          ]
        }],
      ]
    },
  ],
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
            'nlib_target': 'crt_init_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'enable_x86_32': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_64'
            ],
            'out_newlib64':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crti.o',
            'objdir_newlib64':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
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
            'nlib_target': 'crt_fini_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'enable_x86_32': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_64'
            ],
            'out_newlib64':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crtn.o',
            'objdir_newlib64':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
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
            'nlib_target': 'crt_init_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'enable_x86_64': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_32'
            ],
            'out_newlib32':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crti.o',
            'objdir_newlib32':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
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
            'nlib_target': 'crt_fini_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'enable_x86_64': 0,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_x86_32'
            ],
            'out_newlib32':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crtn.o',
            'objdir_newlib32':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
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
            'nlib_target': 'crt_init_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_arm'
            ],
            'out_newlib_arm':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/crti.o',
            'objdir_newlib_arm':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
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
            'nlib_target': 'crt_fini_dummy',
            'windows_asm_rule': 0,
            'build_glibc': 0,
            'build_newlib': 1,
            'extra_args': [
              '--compile',
              '--no-suffix',
              '--strip=_arm'
            ],
            'out_newlib_arm':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/crtn.o',
            'objdir_newlib_arm':
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
          },
          'sources': [
            'src/untrusted/stubs/crtn_arm.S'
          ],
        }
      ],
    }],
  ],
}

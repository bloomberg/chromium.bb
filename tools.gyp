# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'targets' : [
    {
      'target_name': 'prep_toolchain',
      'type': 'none',
      'dependencies': [
        'untar_toolchains',
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
      ],
    },
    {
      'target_name': 'untar_toolchains',
      'type': 'none',
      'variables': {
        'disable_glibc%': 0,
        'disable_newlib%': 0,
        'disable_pnacl%': 0,
      },
      'conditions': [
        ['disable_newlib==0', {
          'actions': [
            {
              'action_name': 'Untar newlib',
              'msvs_cygwin_shell': 0,
              'description': 'Untar newlib',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/naclsdk_<(OS)_x86.tgz',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'newlib',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/naclsdk_<(OS)_x86.tgz',
              ],
            },
          ]
        }],
        ['disable_glibc==0', {
          'actions': [
            {
              'action_name': 'Untar glibc',
              'msvs_cygwin_shell': 0,
              'description': 'Untar glibc',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/toolchain_<(OS)_x86.tar.bz2',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'glibc',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/toolchain_<(OS)_x86.tar.bz2',
              ],
            },
          ]
        }],
        ['disable_pnacl==0', {
          'actions': [
            {
              'action_name': 'Untar pnacl',
              'msvs_cygwin_shell': 0,
              'description': 'Untar pnacl',
              'inputs': [
                 '<(DEPTH)/native_client/build/cygtar.py',
                 '<(DEPTH)/native_client/toolchain/.tars/naclsdk_pnacl_<(OS)_x86.tgz',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/stamp.untar'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/untar_toolchain.py',
                '--tool', 'pnacl',
                '--tmp', '<(SHARED_INTERMEDIATE_DIR)/untar',
                '--sdk', '<(SHARED_INTERMEDIATE_DIR)/sdk',
                '--os', '<(OS)',
                '<(DEPTH)/native_client/toolchain/.tars/naclsdk_pnacl_<(OS)_x86.tgz',
              ],
            },
          ]
        }],
      ]
    },
    {
      'target_name': 'copy_headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Install crt1.o 32',
          'msvs_cygwin_shell': 0,
          'description': 'Install crt1.o 32',
          'inputs': [
            'src/untrusted/stubs/crt1.x',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/32/crt1.o',
          ],
          'action': [
            '>(python_exe)',
            '<(DEPTH)/native_client/build/copy_sources.py',
            'src/untrusted/stubs/crt1.x',
            '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/32/crt1.o',
          ],
        },
        {
          'action_name': 'Install crt1.o 64',
          'msvs_cygwin_shell': 0,
          'description': 'Install crt1.o 64',
          'inputs': [
            'src/untrusted/stubs/crt1.x',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crt1.o',
          ],
          'action': [
            '>(python_exe)',
            '<(DEPTH)/native_client/build/copy_sources.py',
            'src/untrusted/stubs/crt1.x',
            '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crt1.o',
          ],
        },
      ],
      'copies': [
        # NEWLIB copies
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include/nacl',
          # Alphabetical order in dst dir for easier verification.
          'files': [
            '<(DEPTH)/native_client/src/shared/platform/nacl_check.h',
            '<(DEPTH)/native_client/src/untrusted/nacl/nacl_dyncode.h',
            '<(DEPTH)/native_client/src/shared/imc/nacl_imc_c.h',
            '<(DEPTH)/native_client/src/shared/imc/nacl_imc.h',
            '<(DEPTH)/native_client/src/include/nacl/nacl_inttypes.h',
            '<(DEPTH)/native_client/src/shared/platform/nacl_log.h',
            '<(DEPTH)/native_client/src/shared/srpc/nacl_srpc.h',
            '<(DEPTH)/native_client/src/shared/platform/nacl_threads.h',
            '<(DEPTH)/native_client/src/shared/platform/refcount_base.h',
            '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.h'
          ],
        },
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'files': [
            '<(DEPTH)/native_client/src/untrusted/pthread/pthread.h',
            '<(DEPTH)/native_client/src/untrusted/pthread/semaphore.h'
          ],
        },
        # GLIBC copies
        {
          # Alphabetical order in dst dir for easier verification.
          'destination': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/include/nacl',
          'files': [
            '<(DEPTH)/native_client/src/shared/platform/nacl_check.h',
            '<(DEPTH)/native_client/src/shared/imc/nacl_imc.h',
            '<(DEPTH)/native_client/src/include/nacl/nacl_inttypes.h',
            '<(DEPTH)/native_client/src/shared/platform/nacl_log.h',
            '<(DEPTH)/native_client/src/shared/srpc/nacl_srpc.h',
            '<(DEPTH)/native_client/src/shared/platform/nacl_threads.h',
            '<(DEPTH)/native_client/src/shared/platform/refcount_base.h',
            '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.h'
          ],
        },
      ],
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
            'copy_headers'
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
            'copy_headers'
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
            'copy_headers'
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
            'copy_headers'
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
  ],
}

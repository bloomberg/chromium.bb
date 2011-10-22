# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'service_runtime_x86_64',
      'type': 'static_library',
      'variables': {
        'win_target': 'x64',
      },
      'sources': [
        'nacl_app_64.c',
        'nacl_switch_64.S',
        'nacl_switch_to_app_64.c',
        'nacl_syscall_64.S',
        'nacl_tls_64.c',
        'sel_addrspace_x86_64.c',
        'sel_ldr_x86_64.c',
        'sel_rt_64.c',
        'tramp_64.S',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources' : [
            '../../osx/nacl_signal_64.c',
            'sel_addrspace_posix_x86_64.c',
          ] },
        ],
        ['OS=="linux"', {
          'sources' : [
            '../../linux/nacl_signal_64.c',
            'sel_addrspace_posix_x86_64.c',
          ] },
        ],
        ['OS=="win"', {
          'sources' : [
            '../../win/exception_patch/exit_fast.S',
            '../../win/exception_patch/intercept.S',
            '../../win/exception_patch/ntdll_patch.c',
            '../../win/nacl_signal_64.c',
            'sel_addrspace_win_x86_64.c',
            'fxsaverstor.S',
          ] },
        ],
        [ 'library=="shared_library"', {
            'asflags': [
              '-fPIC',
            ],
        }],
      ],
    },
  ],
}

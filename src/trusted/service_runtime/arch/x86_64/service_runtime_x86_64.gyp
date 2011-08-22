# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
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
            '../../win/exception_patch/ntdll_patch.c',
            '../../win/nacl_signal_64.c',
            'sel_addrspace_win_x86_64.c',
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

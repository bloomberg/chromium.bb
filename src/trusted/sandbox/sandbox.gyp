# Copyright 2008, Google Inc.
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
  'variables': {
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
  },
  'conditions': [
    ['OS=="linux"', { 'targets': [
      {
        'target_name': 'sandbox',
        'type': 'static_library',
        'sources': [
          'linux/nacl_syscall_filter.cc',
          'linux/nacl_syscall_checker.cc',
          'linux/nacl_syscall_def.cc',
          'linux/nacl_registers.cc',
          'linux/nacl_sandbox.cc',
        ],
        # TODO(neha): eliminate the need for the warning flag removals below
        'cflags!': [
          '-Wextra',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
          ]
        },
      },
      {
        'target_name': 'sel_ldr_trace',
        'type': 'executable',
        'sources': [
          'linux/nacl_sandbox_main.cc',
        ],
        # libraries?
        # EXTRA_LIBS=['sandbox', '${OPTIONAL_COVERAGE_LIBS}']
      },
    ]}],
    ['OS=="mac"', { 'targets': [ ] } ],
    ['OS=="win"', { 'targets': [ ] } ],
  ],
}

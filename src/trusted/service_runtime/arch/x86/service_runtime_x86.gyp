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
      'target_name': 'service_runtime_x86',
      'type': 'static_library',
      'sources': [
        'nacl_app.c',
        'nacl_ldt_x86.c',
        'nacl_switch_to_app.c',
        'sel_rt.c',
        'nacl_tls.c',
        'sel_ldr_x86.c',
        'sel_addrspace_x86.c',
      ],
      'dependencies': [
        '../../service_runtime.gyp:tramp_gen',
        '../../service_runtime.gyp:springboard_gen',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'actions': [
        {
          'action_name': 'header_gen',
          'conditions': [
            ['OS=="win"', {
              'msvs_cygwin_shell': 0,
            }],
          ],
          'inputs': [
            '<(PRODUCT_DIR)/tramp_gen',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/gen/native_client/src/trusted/service_runtime/arch/x86/tramp_data.h',
          ],
          'action':
            ['<@(_inputs)', '>', '<@(_outputs)'],
          'process_outputs_as_sources': 1,
          'message': 'Creating tramp_data.h',
        },
        {
          'action_name': 'sheader_gen',
          'conditions': [
            ['OS=="win"', {
              'msvs_cygwin_shell': 0,
            }],
          ],
          'inputs': [
            '<(PRODUCT_DIR)/springboard_gen',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/gen/native_client/src/trusted/service_runtime/arch/x86/springboard_data.h',
          ],
          'action':
            ['<@(_inputs)', '>', '<@(_outputs)'],
          'process_outputs_as_sources': 1,
          'message': 'Creating springboard_data.h',
        },
      ],
    },
  ],
}

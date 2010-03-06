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
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="srt_x86_cmn"', {
        'sources': [
          'nacl_ldt_x86.c',
          'sel_validate_image.c',
        ],
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'service_runtime_x86_common',
      'type': 'static_library',
      'variables': {
        'target_base': 'srt_x86_cmn',
      },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'service_runtime_x86_common64',
          'type': 'static_library',
          'variables': {
            'target_base': 'srt_x86_cmn',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate_gen64',
          ],
        },
      ],
    }],
  ],
}

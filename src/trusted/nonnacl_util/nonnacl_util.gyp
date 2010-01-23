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

# TODO(sehr): remove need for the warning flag removals below
{
  'includes': [
    # NOTE: this file also defines common dependencies.
    'nonnacl_util.gypi',
  ],
  'targets': [
    {
      'target_name': 'sel_ldr_launcher',
      'type': 'static_library',
      'sources': [
        'sel_ldr_launcher.cc',
        'sel_ldr_launcher.h',
      ],
      'cflags!': [
        '-Wextra',
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-pedantic',  # import is a gcc extension
          '-Wextra',
        ]
      },
    },
    {
      'target_name': 'nonnacl_util',
      'type': 'static_library',
      'dependencies': [
        'sel_ldr_launcher',
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-pedantic',  # import is a gcc extension
        ]
      },
    },
    {
      'target_name': 'nonnacl_util_c',
      'type': 'static_library',
      'sources': [
        'sel_ldr_launcher_c.cc',
        'sel_ldr_launcher_c.h',
      ],
      'cflags!': [
        '-Wextra',
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-pedantic',  # import is a gcc extension
          '-Wextra',
        ]
      },
      'dependencies': [
        'sel_ldr_launcher',
      ],
    },
  ],
  'conditions': [
    ['nacl_standalone==0', {
      'targets': [
        {
          'target_name': 'nonnacl_util_chrome',
          'type': 'static_library',
          'sources': [
            'sel_ldr_launcher_chrome.cc',
          ],
          'dependencies': [
            'sel_ldr_launcher',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:browserhandle',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# TODO:
# if env.Bit('linux'):
#     env.Append(
#         CCFLAGS=['-fPIC'],
#     )
#if env.Bit('mac'):
#    # there are some issue with compiling ".mm" files
#    env.FilterOut(CCFLAGS=['-pedantic'])
#if env.Bit('windows'):
#    env.Append(
#        CCFLAGS = ['/EHsc'],
#        CPPDEFINES = ['XP_WIN', 'WIN32', '_WINDOWS'],
#    )
#    env.Tool('atlmfc_vc80')
#

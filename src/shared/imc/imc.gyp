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
    'common_sources': [
      'nacl_imc_common.cc',
      'nacl_imc.h',
      'nacl_htp.cc',
      'nacl_htp.h',
    ],
    'conditions': [
      ['OS=="linux"', {
        'common_sources': [
          'nacl_imc_unistd.cc',
          'linux/nacl_imc.cc',
        ],
      }],
      ['OS=="mac"', {
        'common_sources': [
          'nacl_imc_unistd.cc',
          'osx/nacl_imc.cc',
        ],
      }],
      ['OS=="win"', {
        'common_sources': [
          'win/nacl_imc.cc',
          'win/nacl_shm.cc',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '2',  # /EHsc
          },
        },
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
  },
  'targets': [
    {
      'target_name': 'google_nacl_imc',
      'type': 'static_library',
      'sources': [
        '<@(common_sources)',
      ],
    },
    {
      'target_name': 'libgoogle_nacl_imc_c',
      'type': 'static_library',
      'sources': [
        '<@(common_sources)',
        # TODO env_no_strict_aliasing.ComponentObject('nacl_htp_c.cc')
        'nacl_htp_c.cc',
        'nacl_htp_c.h',
        'nacl_imc_c.cc',
        'nacl_imc_c.h',
      ],
    }
  ]
}

# TODO:
# Currently, this is only defined for x86, so only compile if x86.
# if env['TARGET_ARCHITECTURE'] != 'x86':
#    Return()
#
#env_no_strict_aliasing = env.Clone()
#if env.Bit('linux'):
#   env_no_strict_aliasing.Append(CCFLAGS = ['-fno-strict-aliasing'])
#
#env.ComponentProgram('client', 'nacl_imc_test_client.cc')
#env.ComponentProgram('server', 'nacl_imc_test_server.cc')
#
#sigpipe_test_exe = env.ComponentProgram('sigpipe_test', ['sigpipe_test.cc'],
#                                        EXTRA_LIBS=['platform', 'gio'])
#node = env.CommandTestAgainstGoldenOutput(
#    'sigpipe_test.out',
#    [sigpipe_test_exe])
#env.AddNodeToTestSuite(node, ['small_tests'], 'run_imc_tests')
#

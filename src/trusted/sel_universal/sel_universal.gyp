# -*- python -*-
# Copyright 2010, Google Inc.
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
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'sel_universal',
      'type': 'executable',
      'sources': [
        'sel_universal_new.cc',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util_c',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'sel_universal64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'sel_universal_new.cc',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util_c64',
          ],
        },
      ],
    }],
  ],
}

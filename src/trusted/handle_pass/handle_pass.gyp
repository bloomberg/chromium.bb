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
  'defines': [
    'XP_WIN',
    'WIN32',
    '_WINDOWS'
  ],
  'targets': [
    {
      'target_name': 'handle_lookup',
      'type': 'static_library',
      'sources': [
        'handle_lookup.cc',
        'handle_lookup.h',
      ],
    },
    {
      'target_name': 'browserhandle',
      'type': 'static_library',
      'sources': [
        'browser_handle.cc',
        'browser_handle.h',
      ],
      'dependencies': [
        'handle_lookup',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
    {
      'target_name': 'ldrhandle',
      'type': 'static_library',
      'sources': [
        'ldr_handle.cc',
        'ldr_handle.h',
      ],
      'dependencies': [
        'handle_lookup',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    }
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'handle_lookup64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'handle_lookup.cc',
            'handle_lookup.h',
          ],
        },
        {
          'target_name': 'browserhandle64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'browser_handle.cc',
            'browser_handle.h',
          ],
          'dependencies': [
            'handle_lookup64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
        {
          'target_name': 'ldrhandle64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'ldr_handle.cc',
            'ldr_handle.h',
          ],
          'dependencies': [
            'handle_lookup64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        }
      ],
    }],
  ],
}

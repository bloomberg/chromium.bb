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
  'variables': {
    'common_sources': [
      # TODO: we should put a scons file in src/third_party_mod/nacl_plugin
      # which exports a library which is then linked in.
      # Currently this results inlink time symbol clashes
      '<(DEPTH)/native_client/src/third_party_mod/npapi_plugin/np_entry.cc',
      '<(DEPTH)/native_client/src/third_party_mod/npapi_plugin/npn_gate.cc',
      'npp_gate.cc',
      'npp_launcher.cc',
      # SRPC support
      'srpc/srpc.cc',
      'srpc/npapi_native.cc',
      'srpc/plugin.cc',
      'srpc/ret_array.cc',
      'srpc/connected_socket.cc',
      'srpc/multimedia_socket.cc',
      'srpc/shared_memory.cc',
      'srpc/socket_address.cc',
      'srpc/srpc_client.cc',
      'srpc/service_runtime_interface.cc',
      'srpc/srt_socket.cc',
      'srpc/browser_interface.cc',
      'srpc/portable_handle.cc',
      'srpc/desc_based_handle.cc',
      'srpc/closure.cc',
      'srpc/method_map.cc',
      # generic URL-origin / same-domain handling
      'origin.cc',
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'dependencies': [
      '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
      '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
      '<(DEPTH)/native_client/src/shared/imc/imc.gyp:libgoogle_nacl_imc_c',
      '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio',
      '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration',
      ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
          'MOZ_X11',
        ],
        'cflags': [
          '-Wno-long-long',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'XP_MACOSX',
          'XP_UNIX',
          'TARGET_API_MAC_CARBON=1',
          'NO_X11',
          'USE_SYSTEM_CONSOLE',
        ],
        'cflags': [
          '-Wno-long-long',
        ],
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
          ],
        },
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
        'flags': [
          '-fPIC',
          '-Wno-long-long',
        ],
        'link_settings': {
          'libraries': [
            '-lgdi32.lib',
            '-luser32.lib',
          ],
        },
      }],
    ],
  },
}

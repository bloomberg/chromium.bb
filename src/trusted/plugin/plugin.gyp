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
      # TODO: we should put a scons file in src/third_party_mod/nacl_plugin
      # which exports a library which is then linked in.
      # Currently this results inlink time symbol clashes
      '../../third_party_mod/npapi_plugin/np_entry.cc',
      '../../third_party_mod/npapi_plugin/npn_gate.cc',
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
      'srpc/video.cc',
      'srpc/closure.cc',
      'srpc/method_map.cc',
      # generic URL-origin / same-domain handling
      'origin.cc',
    ],
    'conditions': [
      ['OS=="win"', {
        'common_libraries': [
          '-lgdi32.lib',
          '-luser32.lib',
        ],
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
      'target_name': 'npGoogleNaClPlugin',
      'type': 'shared_library',
      'dependencies': [
        '../nonnacl_util/nonnacl_util.gyp:*',
        '../../shared/srpc/srpc.gyp:*',
        '../desc/desc.gyp:*',
        '../../shared/npruntime/npruntime.gyp:*',
        '../../shared/imc/imc.gyp:*',
        '../platform/platform.gyp:*',
        # add gio.lib and expiration.lib when we have gyp files for them
      ],
      'link_settings': {
        'libraries': [  # TODO(gregoryd): this is windows-only, fix
          '<@(common_libraries)',
          '-lnonnacl_util.lib',
          '-lnonnacl_srpc.lib',
          '-lnrd_xfer.lib',
          '-lgoogle_nacl_npruntime.lib',
          '-llibgoogle_nacl_imc_c.lib',
          '-lplatform.lib',
          '-lgio.lib',
          '-lexpiration.lib',
        ],
      },
      'sources': [
        '<@(common_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'nacl_plugin.def',
            'nacl_plugin.rc',
          ],
          'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '2',  # /EHsc
          },
          'VCLinkerTool': {
            'AdditionalLibraryDirectories': [
              '$(OutDir)/lib',
              '$(OutDir)/../../scons-out/dbg-win-x86-32/lib',
            ],
            #['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib'],
          },
        },
        }],
      ],
    },
    {
      # static library for linking with Chrome
      'target_name': 'npGoogleNaClPlugin2',
      'type': 'static_library',
      'defines': [
        'CHROME_BUILD',
      ],
      'sources': [
        '<@(common_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
        }],
      ],
    },
  ]
}

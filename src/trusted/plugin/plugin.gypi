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
      # generic URL-origin / same-domain handling
      'origin.cc',
      # Portable plugin code
      'browser_interface.cc',
      'connected_socket.cc',
      'desc_based_handle.cc',
      'method_map.cc',
      'nexe_arch.cc',
      'plugin.cc',
      'portable_handle.cc',
      'scriptable_handle.cc',
      'service_runtime.cc',
      'shared_memory.cc',
      'socket_address.cc',
      'srpc_client.cc',
      'srt_socket.cc',
      'stream_shm_buffer.cc',
      'string_encoding.cc',
      'utility.cc',
    ],
    'npapi_sources': [
      # NPAPI specific code
      'npapi/async_receive.cc',
      'npapi/browser_impl_npapi.cc',
      'npapi/closure.cc',
      'npapi/multimedia_socket.cc',
      'npapi/npapi_native.cc',
      'npapi/npp_gate.cc',
      'npapi/npp_launcher.cc',
      'npapi/plugin_npapi.cc',
      'npapi/ret_array.cc',
      'npapi/scriptable_impl_npapi.cc',
    ],
    'ppapi_sources': [
      # PPAPI specific code
      'ppapi/array_ppapi.cc',
      'ppapi/browser_interface_ppapi.cc',
      'ppapi/plugin_ppapi.cc',
      'ppapi/file_downloader.cc',
      'ppapi/scriptable_handle_ppapi.cc',
      'ppapi/var_utils.cc',
    ],
    # Append the arch-specific ISA code to common_sources.
    'conditions': [
      # Note: this test assumes that if this is not an ARM build, then this is
      # is an x86 build.  This is because |target_arch| for x86 can be one of a
      # number of values (x64, ia32, etc.).
      ['target_arch=="arm"', {
        'common_sources': [
          'arch_arm/sandbox_isa.cc',
        ],
      }, {  # else: 'target_arch != "arm"
        'common_sources': [
          'arch_x86/sandbox_isa.cc',
        ],
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_platform': 'none',
    },
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
          'MOZ_X11',
        ],
        'cflags': [
          '-Wno-long-long',
        ],
        'ldflags': [
          # Catch unresolved symbols.
          '-Wl,-z,defs',
        ],
        'libraries': [
          '-ldl',
          '-lX11',
          '-lXt',
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
        'sources': [
          'osx/open_mac_file.cc',
          'osx/open_mac_file.h',
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

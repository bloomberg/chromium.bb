# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'common_sources': [
      # TODO: If we decide to never support another plugin system  we should
      # flatten the ppapi directory into the main plugin directory and remove
      # this 'common' set of sources.
      # TODO: we should put a scons file in src/third_party_mod/nacl_plugin
      # which exports a library which is then linked in.
      # Currently this results inlink time symbol clashes
      # Portable plugin code
      'desc_based_handle.cc',
      'method_map.cc',
      'nacl_subprocess.cc',
      'nexe_arch.cc',
      'plugin.cc',
      'portable_handle.cc',
      'scriptable_handle.cc',
      'service_runtime.cc',
      'srpc_client.cc',
      'string_encoding.cc',
      'utility.cc',
    ],
    'ppapi_sources': [
      # PPAPI specific code
      'ppapi/array_ppapi.cc',
      'ppapi/async_receive.cc',
      'ppapi/browser_interface_ppapi.cc',
      'ppapi/plugin_ppapi.cc',
      'ppapi/manifest.cc',
      'ppapi/module_ppapi.cc',
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

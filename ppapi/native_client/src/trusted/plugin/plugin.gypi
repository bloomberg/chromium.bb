# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'common_sources': [
      'file_downloader.cc',
      'json_manifest.cc',
      'local_temp_file.cc',
      'module_ppapi.cc',
      'nacl_subprocess.cc',
      'nexe_arch.cc',
      'plugin.cc',
      'pnacl_coordinator.cc',
      'pnacl_resources.cc',
      'pnacl_translate_thread.cc',
      'scriptable_plugin.cc',
      'sel_ldr_launcher_chrome.cc',
      'service_runtime.cc',
      'srpc_client.cc',
      'srpc_params.cc',
      'utility.cc',
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
    '../../../../../native_client/build/common.gypi',
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
        'cflags!': [
          '-Wno-unused-parameter', # be a bit stricter to match NaCl flags.
        ],
        'conditions': [
          ['asan!=1', {
            'ldflags': [
              # Catch unresolved symbols.
              '-Wl,-z,defs',
            ],
          }],
        ],
        'libraries': [
          '-ldl',
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
        'cflags!': [
          '-Wno-unused-parameter', # be a bit stricter to match NaCl flags.
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

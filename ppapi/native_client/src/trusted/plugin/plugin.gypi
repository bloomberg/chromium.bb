# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'common_sources': [
      'module_ppapi.cc',
      'nacl_subprocess.cc',
      'plugin.cc',
      'pnacl_coordinator.cc',
      'pnacl_resources.cc',
      'pnacl_translate_thread.cc',
      'ppapi_entrypoints.cc',
      'sel_ldr_launcher_chrome.cc',
      'service_runtime.cc',
      'srpc_client.cc',
      'srpc_params.cc',
      'temporary_file.cc',
      'utility.cc',
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
        'cflags': [
          '-Wno-long-long',
        ],
        'cflags!': [
          '-Wno-unused-parameter', # be a bit stricter to match NaCl flags.
        ],
        'conditions': [
          ['asan!=1 and msan!=1 and ubsan_vptr!=1', {
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

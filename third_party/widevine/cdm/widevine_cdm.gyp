# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'widevine_cdm_version_h_file%': 'widevine_cdm_version.h',
    'widevine_cdm_binary_files%': [],
    'conditions': [
      [ 'branding == "Chrome"', {
        'conditions': [
          [ 'chromeos == 1', {
            'widevine_cdm_version_h_file%':
                'symbols/chromeos/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'binaries/chromeos/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "linux" and chromeos == 0', {
            'widevine_cdm_version_h_file%':
                'symbols/linux/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'binaries/linux/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "mac"', {
            'widevine_cdm_version_h_file%':
                'symbols/mac/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'binaries/mac/<(target_arch)/libwidevinecdm.dylib',
            ],
          }],
          [ 'OS == "win"', {
            'widevine_cdm_version_h_file%':
                'symbols/win/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'binaries/win/<(target_arch)/widevinecdm.dll',
            ],
          }],
        ],
      }],
    ],
  },
  # Always provide a target, so we can put the logic about whether there's
  # anything to be done in this file (instead of a higher-level .gyp file).
  'targets': [
    {
      'target_name': 'widevinecdmadapter',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome" and OS != "android" and OS != "ios"', {
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            'widevine_cdm_version_h',
            'widevine_cdm_binaries',
          ],
          'sources': [
            '<(DEPTH)/webkit/media/crypto/ppapi/cdm_wrapper.cc',
            '<(DEPTH)/webkit/media/crypto/ppapi/cdm/content_decryption_module.h',
            '<(DEPTH)/webkit/media/crypto/ppapi/linked_ptr.h',
          ],
          'conditions': [
            [ 'os_posix == 1 and OS != "mac"', {
              'cflags': ['-fvisibility=hidden'],
              'type': 'loadable_module',
              # Allow the plugin wrapper to find the CDM in the same directory.
              'ldflags': ['-Wl,-rpath=\$$ORIGIN'],
              'conditions': [
                # We have binaries for Linux ia32 & x64 and Chrome OS ARM. The
                # build fails if we add the dependency for Chrome OS ia32 & x64.
                [ 'chromeos == 0 or target_arch == "arm"', {
                  'libraries': [
                    # Copied by widevine_cdm_binaries.
                    '<(PRODUCT_DIR)/libwidevinecdm.so',
                  ],
                }],
              ],
            }],
            [ 'OS == "win" and 0', {
              'type': 'shared_library',
            }],
            [ 'OS == "mac" and 0', {
              'type': 'loadable_module',
              'mac_bundle': 1,
              'product_extension': 'plugin',
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  # Not to strip important symbols by -Wl,-dead_strip.
                  '-Wl,-exported_symbol,_PPP_GetInterface',
                  '-Wl,-exported_symbol,_PPP_InitializeModule',
                  '-Wl,-exported_symbol,_PPP_ShutdownModule'
                ]},
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'widevine_cdm_version_h',
      'type': 'none',
      'copies': [{
        'destination': '<(SHARED_INTERMEDIATE_DIR)',
        'files': [ '<(widevine_cdm_version_h_file)' ],
      }],
    },
    {
      'target_name': 'widevine_cdm_binaries',
      'type': 'none',
      'copies': [{
        # TODO(ddorwin): Do we need a sub-directory? We either need a
        # sub-directory or to rename manifest.json before we can copy it.
        'destination': '<(PRODUCT_DIR)',
        'files': [ '<@(widevine_cdm_binary_files)' ],
      }],
    },
  ],
}

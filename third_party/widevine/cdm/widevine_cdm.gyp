# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Allow widevinecdmadapter to be built in Chromium.
    'variables': {
      'enable_widevine%': 0,
    },
    'enable_widevine%': '<(enable_widevine)',
    'widevine_cdm_version_h_file%': 'widevine_cdm_version.h',
    'widevine_cdm_binary_files%': [],
    'conditions': [
      [ 'branding == "Chrome"', {
        'conditions': [
          [ 'chromeos == 1', {
            'widevine_cdm_version_h_file%':
                'chromeos/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'chromeos/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "linux" and chromeos == 0', {
            'widevine_cdm_version_h_file%':
                'linux/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'linux/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "mac"', {
            'widevine_cdm_version_h_file%':
                'mac/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'mac/<(target_arch)/libwidevinecdm.dylib',
            ],
          }],
          [ 'OS == "win"', {
            'widevine_cdm_version_h_file%':
                'win/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'win/<(target_arch)/widevinecdm.dll',
              'win/<(target_arch)/widevinecdm.dll.lib',
            ],
          }],
        ],
      }],
      [ 'OS == "android"', {
        'widevine_cdm_version_h_file%':
            'android/widevine_cdm_version.h',
      }],
      [ 'branding != "Chrome" and OS != "android" and enable_widevine == 1', {
        # If enable_widevine==1 then create a dummy widevinecdm. On Win/Mac
        # the component updater will get the latest version and use it.
        # Other systems are not currently supported.
        'widevine_cdm_version_h_file%':
            'stub/widevine_cdm_version.h',
      }],
    ],
  },
  'includes': [
    '../../../build/util/version.gypi',
  ],

  # Always provide a target, so we can put the logic about whether there's
  # anything to be done in this file (instead of a higher-level .gyp file).
  'targets': [
    {
      # GN version: //third_party/widevine/cdm:widevinecdmadapter_resources
      'target_name': 'widevinecdmadapter_resources',
      'type': 'none',
      'variables': {
        'output_dir': '.',
        'branding_path': '../../../chrome/app/theme/<(branding_path_component)/BRANDING',
        'template_input_path': '../../../chrome/app/chrome_version.rc.version',
        'extra_variable_files_arguments': [ '-f', 'BRANDING' ],
        'extra_variable_files': [ 'BRANDING' ], # NOTE: matches that above
      },
      'sources': [
        'widevinecdmadapter.ver',
      ],
      'includes': [
        '../../../chrome/version_resource_rules.gypi',
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:widevinecdmadapter
      'target_name': 'widevinecdmadapter',
      'type': 'none',
      'conditions': [
        [ '(branding == "Chrome" or enable_widevine == 1) and enable_pepper_cdms == 1', {
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            '<(DEPTH)/media/media_cdm_adapter.gyp:cdmadapter',
            'widevine_cdm_version_h',
            'widevinecdm',
            'widevinecdmadapter_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/widevinecdmadapter_version.rc',
          ],
          'conditions': [
            [ 'os_posix == 1 and OS != "mac"', {
              'libraries': [
                '-lrt',
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/libwidevinecdm.so',
              ],
            }],
            [ 'OS == "win"', {
              'libraries': [
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/widevinecdm.dll.lib',
              ],
            }],
            [ 'OS == "mac"', {
              'libraries': [
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/libwidevinecdm.dylib',
              ],
            }],
          ],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:version_h
      'target_name': 'widevine_cdm_version_h',
      'type': 'none',
      'copies': [{
        'destination': '<(SHARED_INTERMEDIATE_DIR)',
        'files': [ '<(widevine_cdm_version_h_file)' ],
      }],
    },
    {
      # GN version: //third_party/widevine/cdm:widevinecdm
      'target_name': 'widevinecdm',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome"', {
          'conditions': [
            [ 'OS=="mac"', {
              'xcode_settings': {
                'COPY_PHASE_STRIP': 'NO',
              }
            }],
          ],
          'copies': [{
            # TODO(ddorwin): Do we need a sub-directory? We either need a
            # sub-directory or to rename manifest.json before we can copy it.
            'destination': '<(PRODUCT_DIR)',
            'files': [ '<@(widevine_cdm_binary_files)' ],
          }],
        }],
        [ 'branding != "Chrome" and enable_widevine == 1', {
          'conditions': [
            ['os_posix == 1 and OS != "mac"', {
              'type': 'loadable_module',
              # Note that this causes the binary to be put in PRODUCT_DIR
              # instead of lib/. This matches what happens in the copy step
              # above.
            }],
            ['OS == "mac" or OS == "win"', {
              'type': 'shared_library',
            }],
            ['OS == "mac"', {
              'xcode_settings': {
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
              },
            }],
          ],
          'defines': ['CDM_IMPLEMENTATION'],
          'dependencies': [
            'widevine_cdm_version_h',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            '<(DEPTH)/media/cdm/stub/stub_cdm.cc',
            '<(DEPTH)/media/cdm/stub/stub_cdm.h',
          ],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:widevine_test_license_server
      'target_name': 'widevine_test_license_server',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome" and OS == "linux"', {
          'dependencies': [
            '<(DEPTH)/third_party/widevine/test/license_server/license_server.gyp:test_license_server',
          ],
        }],
      ],
    },
  ],
}

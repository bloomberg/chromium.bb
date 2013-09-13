# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win"', {
      'variables': {
        'chromium_code': 1,
      },
      'includes': [
        '../../build/win_precompile.gypi',
        '../../chrome/version.gypi',
      ],
      'target_defaults': {
        'msvs_settings': {
            'VCLinkerTool': {
                'AdditionalDependencies': [
                    'D2D1.lib',
                    'D3D11.lib',
                ],
            },
        },
      },
      'targets': [
        {
          'target_name': 'metro_driver_version_resources',
          'type': 'none',
          'conditions': [
            ['branding == "Chrome"', {
              'variables': {
                 'branding_path': '../../chrome/app/theme/google_chrome/BRANDING',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_path': '../../chrome/app/theme/chromium/BRANDING',
              },
            }],
          ],
          'variables': {
            'output_dir': 'metro_driver',
            'template_input_path': '../../chrome/app/chrome_version.rc.version',
          },
          'sources': [
            'metro_driver_dll.ver',
          ],
          'includes': [
            '../../chrome/version_resource_rules.gypi',
          ],
        },
        {
          'target_name': 'metro_driver',
          'type': 'shared_library',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../chrome/common_constants.gyp:common_constants',
            '../../chrome/chrome.gyp:installer_util',
            '../../crypto/crypto.gyp:crypto',
            '../../google_update/google_update.gyp:google_update',
            '../../ipc/ipc.gyp:ipc',
            '../../sandbox/sandbox.gyp:sandbox',
            '../../ui/metro_viewer/metro_viewer.gyp:metro_viewer_messages',
            '../../url/url.gyp:url_lib',
            '../win8.gyp:check_sdk_patch',
            'metro_driver_version_resources',
          ],
          'sources': [
            'metro_driver.cc',
            'metro_driver.h',
            'stdafx.h',
            'winrt_utils.cc',
            'winrt_utils.h',
            '<(SHARED_INTERMEDIATE_DIR)/metro_driver/metro_driver_dll_version.rc',
          ],
          'conditions': [
            ['use_aura==1', {
              'dependencies': [
                '../win8.gyp:metro_viewer',
              ],
              'sources': [
                'chrome_app_view_ash.cc',
                'chrome_app_view_ash.h',
                'direct3d_helper.cc',
                'direct3d_helper.h',
                'file_picker_ash.cc',
                'file_picker_ash.h',
              ],
            }, {  # use_aura!=1
              'sources': [
                'chrome_app_view.cc',
                'chrome_app_view.h',
                'chrome_url_launch_handler.cc',
                'chrome_url_launch_handler.h',
                'devices_handler.cc',
                'devices_handler.h',
                'file_picker.cc',
                'file_picker.h',
                'metro_dialog_box.cc',
                'metro_dialog_box.h',
                'print_document_source.cc',
                'print_document_source.h',
                'print_handler.cc',
                'print_handler.h',
                'secondary_tile.cc',
                'secondary_tile.h',
                'settings_handler.cc',
                'settings_handler.h',
                'toast_notification_handler.cc',
                'toast_notification_handler.h',
              ],
            }],
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'resources/Logo.png',
                'resources/SecondaryTile.png',
                'resources/SmallLogo.png',
                'resources/splash-620x300.png',
                'resources/VisualElementsManifest.xml',
              ],
            },
          ],
        },
        {
          'target_name': 'metro_driver_unittests',
          'type': 'executable',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../chrome/chrome.gyp:installer_util',
            '../../testing/gtest.gyp:gtest',
            'metro_driver',
          ],
          'sources': [
            'run_all_unittests.cc',
            'winrt_utils.cc',
            'winrt_utils.h',
            'winrt_utils_unittest.cc',
          ],
        },
      ],
    },],
  ],
}

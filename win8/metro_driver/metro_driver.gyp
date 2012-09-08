# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win" and (MSVS_VERSION=="2010" or MSVS_VERSION=="2010e")', {
      'variables': {
        'chromium_code': 1,
      },
      'includes': [
        '../../build/win_precompile.gypi',
      ],
      'target_defaults': {
        'defines': [
          # This define is required to pull in the new Win8 interfaces from
          # system headers like ShObjIdl.h
          'NTDDI_VERSION=0x06020000',
        ],
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
          'target_name': 'metro_driver',
          'type': 'shared_library',
          'dependencies': [
            '../../base/base.gyp:base',
	    '../../build/temp_gyp/googleurl.gyp:googleurl',
            '../../crypto/crypto.gyp:crypto',
            '../../sandbox/sandbox.gyp:sandbox',
            '../../google_update/google_update.gyp:google_update',
            '../win8.gyp:check_sdk_patch',
          ],
          'sources': [
            'chrome_app_view.cc',
            'chrome_app_view.h',
            'chrome_url_launch_handler.cc',
            'chrome_url_launch_handler.h',
            '../delegate_execute/chrome_util.cc',
            '../delegate_execute/chrome_util.h',
            'devices_handler.cc',
            'devices_handler.h',
            'file_picker.h',
            'file_picker.cc',
            'metro_dialog_box.cc',
            'metro_dialog_box.h',
            'metro_driver.cc',
            'print_handler.cc',
            'print_handler.h',
            'print_document_source.cc',
            'print_document_source.h',
            'secondary_tile.h',
            'secondary_tile.cc',
            'settings_handler.cc',
            'settings_handler.h',
            'stdafx.h',
            'toast_notification_handler.cc',
            'toast_notification_handler.h',
            'winrt_utils.cc',
            'winrt_utils.h',
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

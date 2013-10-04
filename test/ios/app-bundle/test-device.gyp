# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
  ],
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test App Gyp',
      'type': 'executable',
      'product_extension': 'bundle',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
      ],
      'mac_bundle_resources': [
        'TestApp/English.lproj/InfoPlist.strings',
        'TestApp/English.lproj/MainMenu.xib',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'INFOPLIST_FILE': 'TestApp/TestApp-Info.plist',
        'IPHONEOS_DEPLOYMENT_TARGET': '4.2',
      },
    },
  ],
}

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'dep_framework',
      'product_name': 'Dependency Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'empty.c', ],
    },
    {    
      'target_name': 'test_framework',
      'product_name': 'Test Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'dependencies': [ 'dep_framework', ],
      'sources': [
        'TestFramework/ObjCVector.h',
        'TestFramework/ObjCVectorInternal.h',
        'TestFramework/ObjCVector.mm',
      ],
      'mac_framework_headers': [
        'TestFramework/ObjCVector.h',
      ],
      'mac_bundle_resources': [
        'TestFramework/English.lproj/InfoPlist.strings',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'TestFramework/Info.plist',
        'GCC_DYNAMIC_NO_PIC': 'NO',
      },
    },
  ],
}

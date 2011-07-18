{
  'targets': [
    {    
      'target_name': 'test_framework',
      'product_name': 'TestFramework',
      'type': 'shared_library',
      'mac_bundle': 1,
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

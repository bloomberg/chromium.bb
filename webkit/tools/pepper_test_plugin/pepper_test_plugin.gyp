
{
  'targets': [
    {
      'target_name': 'pepper_test_plugin',
      'type': 'shared_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '../../..',  # Root of Chrome Checkout
      ],
      'defines': [
        'PEPPER_APIS_ENABLED',
      ],
      'conditions': [
        ['OS=="win"', {
          'product_name': 'pepper_test_plugin',
          'msvs_guid': 'EE00E36E-9E8C-4DFB-925E-FBE32CEDB91A',
          'sources': [
            'pepper_test_plugin.def',
            'pepper_test_plugin.rc',
          ],
        }]
      ],
      'sources': [
        'main.cc',
        'plugin_object.cc',
        'plugin_object.h',
        'test_object.cc',
        'test_object.h',
      ],
    }
  ],
}

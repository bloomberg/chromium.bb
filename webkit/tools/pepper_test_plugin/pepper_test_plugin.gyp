
{
  'targets': [
    {
      'target_name': 'pepper_test_plugin',
      'type': 'shared_library',
      'dependencies': [
        '../../../third_party/npapi/npapi.gyp:npapi',
        '../../../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '../../..',  # Root of Chrome Checkout
      ],
      'conditions': [
        ['OS=="win"', {
          'product_name': 'pepper_test_plugin',
          'msvs_guid': 'EE00E36E-9E8C-4DFB-925E-FBE32CEDB91A',
          'msvs_settings': {
          },
          'sources': [
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

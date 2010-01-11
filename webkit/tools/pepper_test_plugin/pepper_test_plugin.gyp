
{
  'targets': [
    {
      'target_name': 'pepper_test_plugin',
      'type': 'shared_library',
      'dependencies': [
        '../../../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '../../..',  # Root of Chrome Checkout
      ],
      'conditions': [
        ['OS=="win"', {
          'product_name': 'pepper_test_plugin',
          'msvs_guid': 'EE00E36E-9E8C-4DFB-925E-FBE32CEDB91A',
          'dependencies': [
            '../../../gpu/gpu.gyp:gles2_demo_lib',
          ],
          'sources': [
            'pepper_test_plugin.def',
            'pepper_test_plugin.rc',
          ],
        }],
        ['OS=="linux" and (target_arch=="x64" or target_arch=="arm")', {
          # Shared libraries need -fPIC on x86-64
          'cflags': ['-fPIC'],
          'defines': ['INDEPENDENT_PLUGIN'],
        }, {
          'dependencies': [
            '../../../base/base.gyp:base',
            '../../../skia/skia.gyp:skia',
          ],
        }],
      ],
      'sources': [
        'command_buffer_pepper.cc',
        'command_buffer_pepper.h',
        'main.cc',
        'plugin_object.cc',
        'plugin_object.h',
        'test_object.cc',
        'test_object.h',
        'event_handler.cc',
        'event_handler.h'
      ],
      'run_as': {
        'action': [
          '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
          '--no-sandbox',
          '--internal-pepper',
          '--enable-gpu-plugin',
          '--load-plugin=$(TargetPath)',
          'file://$(ProjectDir)test_page.html',
        ],
      },
    }
  ],
}

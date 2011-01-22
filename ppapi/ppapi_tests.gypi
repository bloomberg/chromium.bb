# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
     {
      'target_name': 'ppapi_example',
      'dependencies': [
        'ppapi_cpp'
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'example/Info.plist',
      },
      'sources': [
        'example/example.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'product_name': 'ppapi_example',
          'type': 'shared_library',
          'msvs_guid': 'EE00E36E-9E8C-4DFB-925E-FBE32CEDB91B',
          'sources': [
            'example/example.rc',
          ],
          'run_as': {
            'action': [
              '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
              '--register-pepper-plugins=$(TargetPath);application/x-ppapi-example',
              'file://$(ProjectDir)/example/example.html',
            ],
          },
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'product_name': 'ppapi_example',
          'type': 'shared_library',
          'cflags': ['-fvisibility=hidden'],
          # -gstabs, used in the official builds, causes an ICE. Simply remove
          # it.
          'cflags!': ['-gstabs'],
        }],
        ['OS=="mac"', {
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_name': 'PPAPIExample',
          'product_extension': 'plugin',
          'sources+': [
            'example/Info.plist'
          ],
        }],
      ],
      # See README for instructions on how to run and debug on the Mac.
      #'conditions' : [
      #  ['OS=="mac"', {
      #    'target_name' : 'Chromium',
      #    'type' : 'executable',
      #    'xcode_settings' : {
      #      'ARGUMENTS' : '--renderer-startup-dialog --internal-pepper --no-sandbox file://${SRCROOT}/test_page.html'
      #    },
      #  }],
      #],
    },
#    {
#      'target_name': 'ppapi_example_skeleton',
#      'type': 'none',
#      'dependencies': [
#        'ppapi_cpp',
#      ],
#      'export_dependent_setting': ['ppapi_cpp'],
#      'direct_dependent_settings': {
#        'product_name': '>(_target_name)',
#        'conditions': [
#          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
#            'type': 'shared_library',
#            'cflags': ['-fvisibility=hidden'],
#            # -gstabs, used in the official builds, causes an ICE. Simply remove
#            # it.
#            'cflags!': ['-gstabs'],
#          }],
#          # TODO(ppapi authors):  Make the examples build on Windows & Mac
#          ['OS=="win"', {
#            'suppress_wildcard': 1,
#            'type': 'shared_library',
#          }],
#          ['OS=="mac"', {
#            'suppress_wildcard': 1,
#            'type': 'loadable_module',
#          }],
#        ],
#      },
#    },
#    {
#      'target_name': 'ppapi_example_c_stub',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/stub/stub.c',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_cc_stub',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/stub/stub.cc',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_audio',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/audio/audio.cc',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_file_chooser',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/file_chooser/file_chooser.cc',
#      ],
#    },
#    {  
#      'target_name': 'ppapi_example_graphics_2d',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/2d/graphics_2d_example.c',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_paint_manager',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/2d/paint_manager_example.cc',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_scroll',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/2d/scroll.cc',
#      ],
#    },
#    {
#      'target_name': 'ppapi_example_simple_font',
#      'dependencies': [
#        'ppapi_example_skeleton',
#      ],
#      'sources': [
#        'examples/font/simple_font.cc',
#      ],
#    },
    {
      'target_name': 'ppapi_tests',
      'type': 'loadable_module',
      'sources': [
        # Common test files.
        'tests/test_case.cc',
        'tests/test_case.h',
        'tests/testing_instance.cc',
        'tests/testing_instance.h',

        # Test cases.
        'tests/all_c_includes.h',
        'tests/all_cpp_includes.h',
        'tests/arch_dependent_sizes_32.h',
        'tests/arch_dependent_sizes_64.h',
        'tests/test_buffer.cc',
        'tests/test_buffer.h',
        'tests/test_c_includes.c',
        'tests/test_char_set.cc',
        'tests/test_char_set.h',
        'tests/test_class.cc',
        'tests/test_class.h',
        'tests/test_cpp_includes.cc',
        'tests/test_directory_reader.cc',
        'tests/test_directory_reader.h',
        'tests/test_file_io.cc',
        'tests/test_file_io.h',
        'tests/test_file_ref.cc',
        'tests/test_file_ref.h',
        'tests/test_graphics_2d.cc',
        'tests/test_graphics_2d.h',
        'tests/test_image_data.cc',
        'tests/test_image_data.h',
        'tests/test_paint_aggregator.cc',
        'tests/test_paint_aggregator.h',
        'tests/test_scrollbar.cc',
        'tests/test_scrollbar.h',
        'tests/test_struct_sizes.c',
        'tests/test_transport.cc',
        'tests/test_transport.h',
        'tests/test_url_loader.cc',
        'tests/test_url_loader.h',
        'tests/test_url_util.cc',
        'tests/test_url_util.h',
        'tests/test_utils.cc',
        'tests/test_utils.h',
        'tests/test_var.cc',
        'tests/test_var.h',

        # Deprecated test cases.
        'tests/test_instance_deprecated.cc',
        'tests/test_instance_deprecated.h',
        'tests/test_var_deprecated.cc',
        'tests/test_var_deprecated.h',
      ],
      'dependencies': [
        'ppapi_cpp'
      ],
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '_CRT_SECURE_NO_DEPRECATE',
            '_CRT_NONSTDC_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
        }],
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'ppapi_tests',
          'product_extension': 'plugin',
        }],
      ],
# TODO(dmichael):  Figure out what is wrong with the script on Windows and add
#                  it as an automated action.
#      'actions': [
#        {
#          'action_name': 'generate_ppapi_include_tests',
#          'inputs': [],
#          'outputs': [
#            'tests/test_c_includes.c',
#            'tests/test_cc_includes.cc',
#          ],
#          'action': [
#            '<!@(python generate_ppapi_include_tests.py)',
#          ],
#        },
#      ],
    },
    {
      'target_name': 'ppapi_unittests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'msvs_guid': 'C2BD9365-5BD7-44A7-854E-A49E606BE8E4',
      'dependencies': [
        'ppapi_proxy',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:test_support_ipc',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'proxy/run_all_unittests.cc',

        'proxy/plugin_var_tracker_unittest.cc',
      ],
    },    
  ],
}

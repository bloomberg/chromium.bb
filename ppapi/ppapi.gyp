# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'target_defaults': {
    'conditions': [
      # Linux shared libraries should always be built -fPIC.
      #
      # TODO(ajwong): For internal pepper plugins, which are statically linked
      # into chrome, do we want to build w/o -fPIC?  If so, how can we express
      # that in the build system?
      ['OS=="linux" or OS=="openbsd" or OS=="freebsd" or OS=="solaris"', {
        'cflags': ['-fPIC', '-fvisibility=hidden'],

        # This is needed to make the Linux shlib build happy. Without this,
        # -fvisibility=hidden gets stripped by the exclusion in common.gypi
        # that is triggered when a shared library build is specified.
        'cflags/': [['include', '^-fvisibility=hidden$']],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'ppapi_c',
      'type': 'none',
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'c/pp_bool.h',
        'c/pp_completion_callback.h',
        'c/pp_errors.h',
        'c/pp_input_event.h',
        'c/pp_instance.h',
        'c/pp_macros.h',
        'c/pp_module.h',
        'c/pp_point.h',
        'c/pp_rect.h',
        'c/pp_resource.h',
        'c/pp_size.h',
        'c/pp_stdint.h',
        'c/pp_time.h',
        'c/pp_var.h',
        'c/ppb.h',
        'c/ppb_audio.h',
        'c/ppb_audio_config.h',
        'c/ppb_core.h',
        'c/ppb_class.h',
        'c/ppb_graphics_2d.h',
        'c/ppb_image_data.h',
        'c/ppb_instance.h',
        'c/ppb_url_loader.h',
        'c/ppb_url_request_info.h',
        'c/ppb_url_response_info.h',
        'c/ppb_var.h',
        'c/ppp.h',
        'c/ppp_instance.h',

        # Dev interfaces.
        'c/dev/pp_cursor_type_dev.h',
        'c/dev/pp_file_info_dev.h',
        'c/dev/pp_graphics_3d_dev.h',
        'c/dev/pp_video_dev.h',
        'c/dev/ppb_buffer_dev.h',
        'c/dev/ppb_char_set_dev.h',
        'c/dev/ppb_context_3d_dev.h',
        'c/dev/ppb_cursor_control_dev.h',
        'c/dev/ppb_directory_reader_dev.h',
        'c/dev/ppb_file_chooser_dev.h',
        'c/dev/ppb_file_io_dev.h',
        'c/dev/ppb_file_io_trusted_dev.h',
        'c/dev/ppb_file_ref_dev.h',
        'c/dev/ppb_file_system_dev.h',
        'c/dev/ppb_find_dev.h',
        'c/dev/ppb_font_dev.h',
        'c/dev/ppb_fullscreen_dev.h',
        'c/dev/ppb_graphics_3d_dev.h',
        'c/dev/ppb_opengles_dev.h',
        'c/dev/ppb_scrollbar_dev.h',
        'c/dev/ppb_surface_3d_dev.h',
        'c/dev/ppb_testing_dev.h',
        'c/dev/ppb_transport_dev.h',
        'c/dev/ppb_url_util_dev.h',
        'c/dev/ppb_video_decoder_dev.h',
        'c/dev/ppb_widget_dev.h',
        'c/dev/ppb_zoom_dev.h',
        'c/dev/ppp_cursor_control_dev.h',
        'c/dev/ppp_find_dev.h',
        'c/dev/ppp_graphics_3d_dev.h',
        'c/dev/ppp_scrollbar_dev.h',
        'c/dev/ppp_selection_dev.h',
        'c/dev/ppp_printing_dev.h',
        'c/dev/ppp_widget_dev.h',
        'c/dev/ppp_zoom_dev.h',

        # Private interfaces.
        'c/private/ppb_flash.h',
        'c/private/ppb_nacl_private.h',
        'c/private/ppb_pdf.h',

        # Deprecated interfaces.
        'c/dev/deprecated_bool.h',
        'c/dev/ppb_var_deprecated.h',
        'c/dev/ppp_class_deprecated.h',

        # Trusted interfaces.
        'c/trusted/ppb_audio_trusted.h',
        'c/trusted/ppb_image_data_trusted.h',
        'c/trusted/ppb_url_loader_trusted.h',
      ],
    },
    {
      'target_name': 'ppapi_cpp_objects',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpp/audio.cc',
        'cpp/audio.h',
        'cpp/audio_config.cc',
        'cpp/audio_config.h',
        'cpp/common.h',
        'cpp/completion_callback.h',
        'cpp/core.cc',
        'cpp/core.h',
        'cpp/graphics_2d.cc',
        'cpp/graphics_2d.h',
        'cpp/image_data.cc',
        'cpp/image_data.h',
        'cpp/instance.cc',
        'cpp/instance.h',
        'cpp/logging.h',
        'cpp/module.cc',
        'cpp/module.h',
        'cpp/module_impl.h',
        'cpp/non_thread_safe_ref_count.h',
        'cpp/paint_aggregator.cc',
        'cpp/paint_aggregator.h',
        'cpp/paint_manager.cc',
        'cpp/paint_manager.h',
        'cpp/point.h',
        'cpp/rect.cc',
        'cpp/rect.h',
        'cpp/resource.cc',
        'cpp/resource.h',
        'cpp/size.h',
        'cpp/url_loader.cc',
        'cpp/url_loader.h',
        'cpp/url_request_info.cc',
        'cpp/url_request_info.h',
        'cpp/url_response_info.cc',
        'cpp/url_response_info.h',
        'cpp/var.cc',
        'cpp/var.h',

        # Dev interfaces.
        'cpp/dev/buffer_dev.cc',
        'cpp/dev/buffer_dev.h',
        'cpp/dev/context_3d_dev.cc',
        'cpp/dev/context_3d_dev.h',
        'cpp/dev/directory_entry_dev.cc',
        'cpp/dev/directory_entry_dev.h',
        'cpp/dev/directory_reader_dev.cc',
        'cpp/dev/directory_reader_dev.h',
        'cpp/dev/file_chooser_dev.cc',
        'cpp/dev/file_chooser_dev.h',
        'cpp/dev/file_io_dev.cc',
        'cpp/dev/file_io_dev.h',
        'cpp/dev/file_ref_dev.cc',
        'cpp/dev/file_ref_dev.h',
        'cpp/dev/file_system_dev.cc',
        'cpp/dev/file_system_dev.h',
        'cpp/dev/find_dev.cc',
        'cpp/dev/find_dev.h',
        'cpp/dev/font_dev.cc',
        'cpp/dev/font_dev.h',
        'cpp/dev/fullscreen_dev.cc',
        'cpp/dev/fullscreen_dev.h',
        'cpp/dev/graphics_3d_client_dev.cc',
        'cpp/dev/graphics_3d_client_dev.h',
        'cpp/dev/graphics_3d_dev.cc',
        'cpp/dev/graphics_3d_dev.h',
        'cpp/dev/printing_dev.cc',
        'cpp/dev/printing_dev.h',
        'cpp/dev/scrollbar_dev.cc',
        'cpp/dev/scrollbar_dev.h',
        'cpp/dev/selection_dev.cc',
        'cpp/dev/selection_dev.h',
        'cpp/dev/surface_3d_dev.cc',
        'cpp/dev/surface_3d_dev.h',
        'cpp/dev/transport_dev.cc',
        'cpp/dev/transport_dev.h',
        'cpp/dev/url_util_dev.cc',
        'cpp/dev/url_util_dev.h',
        'cpp/dev/video_decoder_dev.cc',
        'cpp/dev/video_decoder_dev.h',
        'cpp/dev/widget_client_dev.cc',
        'cpp/dev/widget_client_dev.h',
        'cpp/dev/widget_dev.cc',
        'cpp/dev/widget_dev.h',
        'cpp/dev/zoom_dev.cc',
        'cpp/dev/zoom_dev.h',

        # Deprecated interfaces.
        'cpp/dev/scriptable_object_deprecated.h',
        'cpp/dev/scriptable_object_deprecated.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': 'AD371A1D-3459-4E2D-8E8A-881F4B83B908',
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': ['/we4244'],  # implicit conversion, possible loss of data
            },
          },
        }],
        ['OS=="linux"', {
          'cflags': ['-Wextra', '-pedantic'],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wextra', '-pedantic'],
           },
        }]
      ],
    },
    {
      'target_name': 'ppapi_cpp',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
        'ppapi_cpp_objects',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpp/module_embedder.h',
        'cpp/ppp_entrypoints.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': '057E7FA0-83C0-11DF-8395-0800200C9A66',
        }],
        ['OS=="linux"', {
          'cflags': ['-Wextra', '-pedantic'],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wextra', '-pedantic'],
           },
        }]
      ],
    },
    {
      'target_name': 'ppapi_gles2',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'lib/gl/gles2/gl2ext_ppapi.c',
        'lib/gl/gles2/gl2ext_ppapi.h',
        'lib/gl/gles2/gles2.c',
      ],
    },
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
      'target_name': 'ppapi_shared_impl',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'shared_impl/audio_impl.cc',
        'shared_impl/audio_impl.h',
        'shared_impl/image_data_impl.cc',
        'shared_impl/image_data_impl.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': 'E7420D65-A885-41EB-B4BE-04DE0C97033B',
        }],
      ],
    },
    {
      'target_name': 'ppapi_proxy',
      'type': 'static_library',
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
        'ppapi_c',
        'ppapi_shared_impl',
      ],
      'all_dependent_settings': {
        'include_dirs': [
           '..',
        ],
      },
      'include_dirs': [
        '..',
        '../..',  # For nacl includes to work.
      ],
      'sources': [
        'proxy/callback_tracker.cc',
        'proxy/callback_tracker.h',
        'proxy/dispatcher.cc',
        'proxy/dispatcher.h',
        'proxy/host_dispatcher.cc',
        'proxy/host_dispatcher.h',
        'proxy/host_var_serialization_rules.cc',
        'proxy/host_var_serialization_rules.h',
        'proxy/image_data.cc',
        'proxy/image_data.h',
        'proxy/interface_proxy.cc',
        'proxy/interface_proxy.h',
        'proxy/plugin_dispatcher.cc',
        'proxy/plugin_dispatcher.h',
        'proxy/plugin_resource.cc',
        'proxy/plugin_resource.h',
        'proxy/plugin_resource_tracker.cc',
        'proxy/plugin_resource_tracker.h',
        'proxy/plugin_var_serialization_rules.cc',
        'proxy/plugin_var_serialization_rules.h',
        'proxy/plugin_var_tracker.cc',
        'proxy/plugin_var_tracker.h',
        'proxy/ppapi_messages.cc',
        'proxy/ppapi_messages.h',
        'proxy/ppapi_messages_internal.h',
        'proxy/ppapi_param_traits.cc',
        'proxy/ppapi_param_traits.h',
        'proxy/ppb_audio_config_proxy.cc',
        'proxy/ppb_audio_config_proxy.h',
        'proxy/ppb_audio_proxy.cc',
        'proxy/ppb_audio_proxy.h',
        'proxy/ppb_buffer_proxy.cc',
        'proxy/ppb_buffer_proxy.h',
        'proxy/ppb_char_set_proxy.cc',
        'proxy/ppb_char_set_proxy.h',
        'proxy/ppb_core_proxy.cc',
        'proxy/ppb_core_proxy.h',
        'proxy/ppb_cursor_control_proxy.cc',
        'proxy/ppb_cursor_control_proxy.h',
        'proxy/ppb_flash_proxy.cc',
        'proxy/ppb_flash_proxy.h',
        'proxy/ppb_font_proxy.cc',
        'proxy/ppb_font_proxy.h',
        'proxy/ppb_fullscreen_proxy.cc',
        'proxy/ppb_fullscreen_proxy.h',
        'proxy/ppb_graphics_2d_proxy.cc',
        'proxy/ppb_graphics_2d_proxy.h',
        'proxy/ppb_image_data_proxy.cc',
        'proxy/ppb_image_data_proxy.h',
        'proxy/ppb_instance_proxy.cc',
        'proxy/ppb_instance_proxy.h',
        'proxy/ppb_pdf_proxy.cc',
        'proxy/ppb_pdf_proxy.h',
        'proxy/ppb_testing_proxy.cc',
        'proxy/ppb_testing_proxy.h',
        'proxy/ppb_url_loader_proxy.cc',
        'proxy/ppb_url_loader_proxy.h',
        'proxy/ppb_url_request_info_proxy.cc',
        'proxy/ppb_url_request_info_proxy.h',
        'proxy/ppb_url_response_info_proxy.cc',
        'proxy/ppb_url_response_info_proxy.h',
        'proxy/ppb_var_deprecated_proxy.cc',
        'proxy/ppb_var_deprecated_proxy.h',
        'proxy/ppp_class_proxy.cc',
        'proxy/ppp_class_proxy.h',
        'proxy/ppp_instance_proxy.cc',
        'proxy/ppp_instance_proxy.h',
        'proxy/serialized_structs.cc',
        'proxy/serialized_structs.h',
        'proxy/serialized_var.cc',
        'proxy/serialized_var.h',
        'proxy/var_serialization_rules.h',
      ],
      'defines': [
      ],
      'conditions': [
        ['OS=="win"', {
        }],
        ['OS=="linux"', {
        }],
        ['OS=="mac"', {
        }]
      ],
    },
  ],
}

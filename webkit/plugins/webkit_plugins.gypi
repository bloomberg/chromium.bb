# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'plugins_common',
      'type': 'static_library',
      'defines': [
        'WEBKIT_PLUGINS_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
      ],
      'sources': [
        '../plugins/webplugininfo.cc',
        '../plugins/webplugininfo.h',
        '../plugins/plugin_constants.cc',
        '../plugins/plugin_constants.h',
        '../plugins/plugin_switches.cc',
        '../plugins/plugin_switches.h',
        '../plugins/npapi/plugin_constants_win.cc',
        '../plugins/npapi/plugin_constants_win.h',
        '../plugins/npapi/plugin_list.cc',
        '../plugins/npapi/plugin_list.h',
        '../plugins/npapi/plugin_list_mac.mm',
        '../plugins/npapi/plugin_list_posix.cc',
        '../plugins/npapi/plugin_list_win.cc',
        '../plugins/npapi/plugin_utils.cc',
        '../plugins/npapi/plugin_utils.h',
        '../common/plugins/ppapi/ppapi_utils.cc',
        '../common/plugins/ppapi/ppapi_utils.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
        }, {  # else: OS=="mac"
          'sources/': [['exclude', 'plugin_list_posix\\.cc$']],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
        }, {  # else: OS=="win"
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },

    {
      'target_name': 'plugins',
      'type': '<(component)',
      'defines': [
        'WEBKIT_PLUGINS_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_c',
        '<(DEPTH)/ppapi/ppapi_internal.gyp:ppapi_shared',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/renderer/compositor_bindings/compositor_bindings.gyp:webkit_compositor_support',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        'glue_common',
        'plugins_common',
        'user_agent',
        'webkit_base',
        'webkit_storage',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        '../plugins/npapi/gtk_plugin_container.cc',
        '../plugins/npapi/gtk_plugin_container.h',
        '../plugins/npapi/gtk_plugin_container_manager.cc',
        '../plugins/npapi/gtk_plugin_container_manager.h',
        '../plugins/npapi/plugin_host.cc',
        '../plugins/npapi/plugin_host.h',
        '../plugins/npapi/plugin_instance.cc',
        '../plugins/npapi/plugin_instance.h',
        '../plugins/npapi/plugin_instance_mac.mm',
        '../plugins/npapi/plugin_lib.cc',
        '../plugins/npapi/plugin_lib.h',
        '../plugins/npapi/plugin_stream.cc',
        '../plugins/npapi/plugin_stream.h',
        '../plugins/npapi/plugin_stream_posix.cc',
        '../plugins/npapi/plugin_stream_url.cc',
        '../plugins/npapi/plugin_stream_url.h',
        '../plugins/npapi/plugin_stream_win.cc',
        '../plugins/npapi/plugin_string_stream.cc',
        '../plugins/npapi/plugin_string_stream.h',
        '../plugins/npapi/plugin_web_event_converter_mac.h',
        '../plugins/npapi/plugin_web_event_converter_mac.mm',
        '../plugins/npapi/webplugin.cc',
        '../plugins/npapi/webplugin.h',
        '../plugins/npapi/webplugin_accelerated_surface_mac.h',
        '../plugins/npapi/webplugin_delegate.h',
        '../plugins/npapi/webplugin_delegate_impl.cc',
        '../plugins/npapi/webplugin_delegate_impl.h',
        '../plugins/npapi/webplugin_delegate_impl_android.cc',
        '../plugins/npapi/webplugin_delegate_impl_aura.cc',
        '../plugins/npapi/webplugin_delegate_impl_gtk.cc',
        '../plugins/npapi/webplugin_delegate_impl_mac.mm',
        '../plugins/npapi/webplugin_delegate_impl_win.cc',
        '../plugins/npapi/webplugin_ime_win.cc',
        '../plugins/npapi/webplugin_ime_win.h',
        '../plugins/npapi/webplugin_impl.cc',
        '../plugins/npapi/webplugin_impl.h',
        '../plugins/ppapi/audio_helper.cc',
        '../plugins/ppapi/audio_helper.h',
        '../plugins/ppapi/common.h',
        '../plugins/ppapi/content_decryptor_delegate.cc',
        '../plugins/ppapi/content_decryptor_delegate.h',
        '../plugins/ppapi/event_conversion.cc',
        '../plugins/ppapi/event_conversion.h',
        '../plugins/ppapi/fullscreen_container.h',
        '../plugins/ppapi/gfx_conversion.h',
        '../plugins/ppapi/host_array_buffer_var.cc',
        '../plugins/ppapi/host_array_buffer_var.h',
        '../plugins/ppapi/host_globals.cc',
        '../plugins/ppapi/host_globals.h',
        '../plugins/ppapi/host_var_tracker.cc',
        '../plugins/ppapi/host_var_tracker.h',
        '../plugins/ppapi/message_channel.cc',
        '../plugins/ppapi/message_channel.h',
        '../plugins/ppapi/npapi_glue.cc',
        '../plugins/ppapi/npapi_glue.h',
        '../plugins/ppapi/npobject_var.cc',
        '../plugins/ppapi/npobject_var.h',
        '../plugins/ppapi/plugin_delegate.h',
        '../plugins/ppapi/plugin_module.cc',
        '../plugins/ppapi/plugin_module.h',
        '../plugins/ppapi/plugin_object.cc',
        '../plugins/ppapi/plugin_object.h',
        '../plugins/ppapi/ppapi_interface_factory.cc',
        '../plugins/ppapi/ppapi_interface_factory.h',
        '../plugins/ppapi/ppapi_plugin_instance.cc',
        '../plugins/ppapi/ppapi_plugin_instance.h',
        '../plugins/ppapi/ppapi_webplugin_impl.cc',
        '../plugins/ppapi/ppapi_webplugin_impl.h',
        '../plugins/ppapi/ppb_audio_impl.cc',
        '../plugins/ppapi/ppb_audio_impl.h',
        '../plugins/ppapi/ppb_broker_impl.cc',
        '../plugins/ppapi/ppb_broker_impl.h',
        '../plugins/ppapi/ppb_buffer_impl.cc',
        '../plugins/ppapi/ppb_buffer_impl.h',
        '../plugins/ppapi/ppb_file_ref_impl.cc',
        '../plugins/ppapi/ppb_file_ref_impl.h',
        '../plugins/ppapi/ppb_flash_message_loop_impl.cc',
        '../plugins/ppapi/ppb_flash_message_loop_impl.h',
        '../plugins/ppapi/ppb_gpu_blacklist_private_impl.cc',
        '../plugins/ppapi/ppb_gpu_blacklist_private_impl.h',
        '../plugins/ppapi/ppb_graphics_3d_impl.cc',
        '../plugins/ppapi/ppb_graphics_3d_impl.h',
        '../plugins/ppapi/ppb_image_data_impl.cc',
        '../plugins/ppapi/ppb_image_data_impl.h',
        '../plugins/ppapi/ppb_network_monitor_private_impl.cc',
        '../plugins/ppapi/ppb_network_monitor_private_impl.h',
        '../plugins/ppapi/ppb_proxy_impl.cc',
        '../plugins/ppapi/ppb_proxy_impl.h',
        '../plugins/ppapi/ppb_scrollbar_impl.cc',
        '../plugins/ppapi/ppb_scrollbar_impl.h',
        '../plugins/ppapi/ppb_tcp_server_socket_private_impl.cc',
        '../plugins/ppapi/ppb_tcp_server_socket_private_impl.h',
        '../plugins/ppapi/ppb_tcp_socket_private_impl.cc',
        '../plugins/ppapi/ppb_tcp_socket_private_impl.h',
        '../plugins/ppapi/ppb_uma_private_impl.cc',
        '../plugins/ppapi/ppb_uma_private_impl.h',
        '../plugins/ppapi/ppb_var_deprecated_impl.cc',
        '../plugins/ppapi/ppb_var_deprecated_impl.h',
        '../plugins/ppapi/ppb_video_decoder_impl.cc',
        '../plugins/ppapi/ppb_video_decoder_impl.h',
        '../plugins/ppapi/ppb_widget_impl.cc',
        '../plugins/ppapi/ppb_widget_impl.h',
        '../plugins/ppapi/ppb_x509_certificate_private_impl.cc',
        '../plugins/ppapi/ppb_x509_certificate_private_impl.h',
        '../plugins/ppapi/quota_file_io.cc',
        '../plugins/ppapi/quota_file_io.h',
        '../plugins/ppapi/resource_creation_impl.cc',
        '../plugins/ppapi/resource_creation_impl.h',
        '../plugins/ppapi/resource_helper.cc',
        '../plugins/ppapi/resource_helper.h',
        '../plugins/ppapi/string.cc',
        '../plugins/ppapi/string.h',
        '../plugins/ppapi/url_response_info_util.cc',
        '../plugins/ppapi/url_response_info_util.h',
        '../plugins/ppapi/url_request_info_util.cc',
        '../plugins/ppapi/url_request_info_util.h',
        '../plugins/ppapi/usb_key_code_conversion.h',
        '../plugins/ppapi/usb_key_code_conversion.cc',
        '../plugins/ppapi/usb_key_code_conversion_linux.cc',
        '../plugins/ppapi/usb_key_code_conversion_mac.cc',
        '../plugins/ppapi/usb_key_code_conversion_win.cc',
        '../plugins/ppapi/v8_var_converter.cc',
        '../plugins/ppapi/v8_var_converter.h',
        '../plugins/sad_plugin.cc',
        '../plugins/sad_plugin.h',
        '../plugins/webkit_plugins_export.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['use_aura==1', {
          'sources/': [
            ['exclude', '^\\.\\./plugins/npapi/webplugin_delegate_impl_mac.mm'],
          ],
        }],
        ['use_aura==1 and OS=="win"', {
          'sources/': [
            ['exclude', '^\\.\\./plugins/npapi/webplugin_delegate_impl_aura'],
          ],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
        }, {  # else: OS=="mac"
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['enable_gpu!=1', {
          'sources!': [
            '../plugins/ppapi/ppb_graphics_3d_impl.cc',
            '../plugins/ppapi/ppb_graphics_3d_impl.h',
            '../plugins/ppapi/ppb_open_gl_es_impl.cc',
          ],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
        }, {  # else: OS=="win"
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_mock_plugin_list',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue'
      ],
      'sources': [
        '../plugins/npapi/mock_plugin_list.cc',
        '../plugins/npapi/mock_plugin_list.h',
      ]
    },
    {
      'target_name': 'pull_in_copy_TestNetscapePlugIn',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp:copy_TestNetscapePlugIn'
      ],
    },
  ],
  'conditions': [
    ['OS!="android" and OS!="ios"', {
      # npapi test plugin doesn't build on android or ios
      'targets': [
        {
          'target_name': 'npapi_test_common',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
          ],
          'sources': [
            '../plugins/npapi/test/npapi_constants.cc',
            '../plugins/npapi/test/npapi_constants.h',
            '../plugins/npapi/test/plugin_client.cc',
            '../plugins/npapi/test/plugin_client.h',
            '../plugins/npapi/test/plugin_test.cc',
            '../plugins/npapi/test/plugin_test.h',
            '../plugins/npapi/test/plugin_test_factory.h',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/base/base.gyp:base',
          ],
        },
        {
          'target_name': 'npapi_test_plugin',
          'type': 'loadable_module',
          'variables': {
            'chromium_code': 1,
          },
          'mac_bundle': 1,
          'dependencies': [
            '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
            'npapi_test_common',
          ],
          'sources': [
            '../plugins/npapi/test/npapi_test.cc',
            '../plugins/npapi/test/npapi_test.def',
            '../plugins/npapi/test/npapi_test.rc',
            '../plugins/npapi/test/plugin_arguments_test.cc',
            '../plugins/npapi/test/plugin_arguments_test.h',
            '../plugins/npapi/test/plugin_create_instance_in_paint.cc',
            '../plugins/npapi/test/plugin_create_instance_in_paint.h',
            '../plugins/npapi/test/plugin_delete_plugin_in_deallocate_test.cc',
            '../plugins/npapi/test/plugin_delete_plugin_in_deallocate_test.h',
            '../plugins/npapi/test/plugin_delete_plugin_in_stream_test.cc',
            '../plugins/npapi/test/plugin_delete_plugin_in_stream_test.h',
            '../plugins/npapi/test/plugin_execute_stream_javascript.cc',
            '../plugins/npapi/test/plugin_execute_stream_javascript.h',
            '../plugins/npapi/test/plugin_get_javascript_url_test.cc',
            '../plugins/npapi/test/plugin_get_javascript_url_test.h',
            '../plugins/npapi/test/plugin_get_javascript_url2_test.cc',
            '../plugins/npapi/test/plugin_get_javascript_url2_test.h',
            '../plugins/npapi/test/plugin_geturl_test.cc',
            '../plugins/npapi/test/plugin_geturl_test.h',
            '../plugins/npapi/test/plugin_javascript_open_popup.cc',
            '../plugins/npapi/test/plugin_javascript_open_popup.h',
            '../plugins/npapi/test/plugin_new_fails_test.cc',
            '../plugins/npapi/test/plugin_new_fails_test.h',
            '../plugins/npapi/test/plugin_npobject_identity_test.cc',
            '../plugins/npapi/test/plugin_npobject_identity_test.h',
            '../plugins/npapi/test/plugin_npobject_lifetime_test.cc',
            '../plugins/npapi/test/plugin_npobject_lifetime_test.h',
            '../plugins/npapi/test/plugin_npobject_proxy_test.cc',
            '../plugins/npapi/test/plugin_npobject_proxy_test.h',
            '../plugins/npapi/test/plugin_schedule_timer_test.cc',
            '../plugins/npapi/test/plugin_schedule_timer_test.h',
            '../plugins/npapi/test/plugin_setup_test.cc',
            '../plugins/npapi/test/plugin_setup_test.h',
            '../plugins/npapi/test/plugin_thread_async_call_test.cc',
            '../plugins/npapi/test/plugin_thread_async_call_test.h',
            '../plugins/npapi/test/plugin_windowed_test.cc',
            '../plugins/npapi/test/plugin_windowed_test.h',
            '../plugins/npapi/test/plugin_private_test.cc',
            '../plugins/npapi/test/plugin_private_test.h',
            '../plugins/npapi/test/plugin_test_factory.cc',
            '../plugins/npapi/test/plugin_window_size_test.cc',
            '../plugins/npapi/test/plugin_window_size_test.h',
            '../plugins/npapi/test/plugin_windowless_test.cc',
            '../plugins/npapi/test/plugin_windowless_test.h',
            '../plugins/npapi/test/resource.h',
          ],
          'include_dirs': [
            '..',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(DEPTH)/webkit/plugins/npapi/test/Info.plist',
          },
          'conditions': [
            ['OS!="win"', {
              'sources!': [
                # TODO(port):  Port these.
                 # plugin_npobject_lifetime_test.cc has win32-isms
                #   (HWND, CALLBACK).
                '../plugins/npapi/test/plugin_npobject_lifetime_test.cc',
                 # The window APIs are necessarily platform-specific.
                '../plugins/npapi/test/plugin_window_size_test.cc',
                '../plugins/npapi/test/plugin_windowed_test.cc',
                 # Seems windows specific.
                '../plugins/npapi/test/plugin_create_instance_in_paint.cc',
                '../plugins/npapi/test/plugin_create_instance_in_paint.h',
                 # windows-specific resources
                '../plugins/npapi/test/npapi_test.def',
                '../plugins/npapi/test/npapi_test.rc',
              ],
            }],
            ['OS=="mac"', {
              'product_extension': 'plugin',
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                ],
              },
            }],
            ['os_posix == 1 and OS != "mac" and (target_arch == "x64" or target_arch == "arm")', {
              # Shared libraries need -fPIC on x86-64
              'cflags': ['-fPIC']
            }],
          ],
        },
        {
          'target_name': 'copy_npapi_test_plugin',
          'type': 'none',
          'dependencies': [
            'npapi_test_plugin',
          ],
          'conditions': [
            ['OS=="win"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)/plugins',
                  'files': ['<(PRODUCT_DIR)/npapi_test_plugin.dll'],
                },
              ],
            }],
            ['OS=="mac"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)/plugins/',
                  'files': ['<(PRODUCT_DIR)/npapi_test_plugin.plugin'],
                },
              ]
            }],
            ['os_posix == 1 and OS != "mac"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)/plugins',
                  'files': ['<(PRODUCT_DIR)/libnpapi_test_plugin.so'],
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

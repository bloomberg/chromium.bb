# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'test_shell_windows_resource_files': [
      'resources/test_shell.rc',
      '../../glue/resources/pan_east.cur',
      '../../glue/resources/pan_middle.cur',
      '../../glue/resources/pan_north.cur',
      '../../glue/resources/pan_north_east.cur',
      '../../glue/resources/pan_north_west.cur',
      '../../glue/resources/pan_south.cur',
      '../../glue/resources/pan_south_east.cur',
      '../../glue/resources/pan_south_west.cur',
      '../../glue/resources/pan_west.cur',
      'resources/small.ico',
      'resources/test_shell.ico',
      'resource.h',
    ],
  },
  'targets': [
    {
      'target_name': 'pull_in_copy_TestNetscapePlugIn',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp:copy_TestNetscapePlugIn'
      ],
    },
    {
      'target_name': 'test_shell_common',
      'type': 'static_library',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:inspector_resources',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:appcache',
        '<(DEPTH)/webkit/support/webkit_support.gyp:blob',
        '<(DEPTH)/webkit/support/webkit_support.gyp:database',
        '<(DEPTH)/webkit/support/webkit_support.gyp:dom_storage',
        '<(DEPTH)/webkit/support/webkit_support.gyp:fileapi',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/webkit/support/webkit_support.gyp:quota',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_gpu',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_media',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support_common',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_user_agent',
      ],
      'sources': [
        'mac/test_shell_webview.h',
        'mac/test_shell_webview.mm',
        'accessibility_ui_element.cc',
        'accessibility_ui_element.h',
        'drop_delegate.cc',
        'drop_delegate.h',
        'layout_test_controller.cc',
        'layout_test_controller.h',
        'mock_spellcheck.cc',
        'mock_spellcheck.h',
        'notification_presenter.cc',
        'notification_presenter.h',
        'resource.h',
        'test_navigation_controller.cc',
        'test_navigation_controller.h',
        'test_shell.cc',
        'test_shell.h',
        'test_shell_devtools_agent.cc',
        'test_shell_devtools_agent.h',
        'test_shell_devtools_callargs.cc',
        'test_shell_devtools_callargs.h',
        'test_shell_devtools_client.cc',
        'test_shell_devtools_client.h',
        'test_shell_gtk.cc',
        'test_shell_x11.cc',
        'test_shell_mac.mm',
        'test_shell_platform_delegate.h',
        'test_shell_platform_delegate_gtk.cc',
        'test_shell_platform_delegate_mac.mm',
        'test_shell_platform_delegate_win.cc',
        'test_shell_switches.cc',
        'test_shell_switches.h',
        'test_shell_win.cc',
        'test_shell_webkit_init.cc',
        'test_shell_webkit_init.h',
        'test_shell_webthemecontrol.h',
        'test_shell_webthemecontrol.cc',
        'test_shell_webthemeengine.h',
        'test_shell_webthemeengine.cc',
        'test_webview_delegate.cc',
        'test_webview_delegate.h',
        'test_webview_delegate_mac.mm',
        'test_webview_delegate_gtk.cc',
        'test_webview_delegate_win.cc',
        'webview_host.h',
        'webview_host_gtk.cc',
        'webview_host_mac.mm',
        'webview_host_win.cc',
        'webwidget_host.h',
        'webwidget_host.cc',
        'webwidget_host_gtk.cc',
        'webwidget_host_mac.mm',
        'webwidget_host_win.cc',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
      ],
      'conditions': [
        ['target_arch!="arm"', {
          'dependencies': [
            'copy_npapi_test_plugin',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            'test_shell_resources',
            '<(DEPTH)/build/linux/system.gyp:gtk',
            '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          # for:  test_shell_gtk.cc
          'cflags': ['-Wno-multichar'],
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
            '.',
          ],
          'dependencies': [
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
          ],
        }, {  # else: OS!=win
          'sources/': [
            ['exclude', '_webtheme(control|engine)\.(cc|h)$'],
          ],
          'sources!': [
            'drop_delegate.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_shell_pak',
      'type': 'none',
      'variables': {
        'repack_path': '../../../tools/grit/grit/format/repack.py',
        'pak_path': '<(INTERMEDIATE_DIR)/repack/test_shell.pak',
      },
      'conditions': [
        ['os_posix == 1 and OS != "mac"', {
          'actions': [
            {
              'action_name': 'test_shell_repack',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/test_shell/test_shell_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(pak_path)',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': ['<(pak_path)'],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'test_shell',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'mac_bundle': 1,
      'dependencies': [
        'test_shell_common',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
        'pull_in_copy_TestNetscapePlugIn',
        '<(DEPTH)/tools/imagediff/image_diff.gyp:image_diff',
      ],
      'defines': [
        # Technically not a unit test but require functions available only to
        # unit tests.
        'UNIT_TEST'
      ],
      'sources': [
        'test_shell_main.cc',
      ],
      'mac_bundle_resources': [
        '../../data/test_shell/',
        'mac/English.lproj/InfoPlist.strings',
        'mac/English.lproj/MainMenu.xib',
        'mac/Info.plist',
        'mac/test_shell.icns',
        'resources/AHEM____.TTF',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': '<(DEPTH)/webkit/tools/test_shell/mac/Info.plist',
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'layout_test_helper',
          ],
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'sources': [
            '<@(test_shell_windows_resource_files)',
            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['toolkit_uses_gtk == 1', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
            'test_shell_resources',
            'test_shell_pak',
          ],
        }],
        ['OS=="mac"', {
          'product_name': 'TestShell',
          'dependencies': [
            'layout_test_helper',
            '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
          ],
          'variables': {
            'repack_path': '../../../tools/grit/grit/format/repack.py',
          },
          'actions': [
            {
              # TODO(mark): Make this work with more languages than the
              # hardcoded en-US.
              'action_name': 'repack_locale',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/test_shell.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
          'copies': [
            {
              # Copy FFmpeg binaries for audio/video support.
              'destination': '<(PRODUCT_DIR)/TestShell.app/Contents/MacOS/',
              'files': [
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
          ],
        }, { # OS != "mac"
          'dependencies': [
            '<(DEPTH)/ui/ui.gyp:gfx_resources',
            '<(DEPTH)/net/net.gyp:net_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
          ]
        }],
      ],
    },
    {
      'target_name': 'test_shell_test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue'
      ],
      'sources': [
        '../../plugins/npapi/mock_plugin_list.cc',
        '../../plugins/npapi/mock_plugin_list.h',
      ]
    },
    {
      'target_name': 'test_shell_tests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        'test_shell_common',
        'test_shell_test_support',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/media/media.gyp:media_test_support',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/ppapi/ppapi_internal.gyp:ppapi_shared',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_user_agent',
      ],
      'sources': [
        '../../../skia/ext/convolver_unittest.cc',
        '../../../skia/ext/image_operations_unittest.cc',
        '../../../skia/ext/platform_canvas_unittest.cc',
        '../../../skia/ext/vector_canvas_unittest.cc',
        '../../appcache/manifest_parser_unittest.cc',
        '../../appcache/appcache_unittest.cc',
        '../../appcache/appcache_database_unittest.cc',
        '../../appcache/appcache_group_unittest.cc',
        '../../appcache/appcache_host_unittest.cc',
        '../../appcache/appcache_quota_client_unittest.cc',
        '../../appcache/appcache_request_handler_unittest.cc',
        '../../appcache/appcache_response_unittest.cc',
        '../../appcache/appcache_service_unittest.cc',
        '../../appcache/appcache_storage_unittest.cc',
        '../../appcache/appcache_storage_impl_unittest.cc',
        '../../appcache/appcache_update_job_unittest.cc',
        '../../appcache/appcache_url_request_job_unittest.cc',
        '../../appcache/mock_appcache_policy.h',
        '../../appcache/mock_appcache_policy.cc',
        '../../appcache/mock_appcache_service.cc',
        '../../appcache/mock_appcache_service.h',
        '../../appcache/mock_appcache_storage.cc',
        '../../appcache/mock_appcache_storage.h',
        '../../appcache/mock_appcache_storage_unittest.cc',
        '../../blob/blob_storage_controller_unittest.cc',
        '../../blob/blob_url_request_job_unittest.cc',
        '../../blob/shareable_file_reference_unittest.cc',
        '../../database/database_connections_unittest.cc',
        '../../database/database_quota_client_unittest.cc',
        '../../database/databases_table_unittest.cc',
        '../../database/database_tracker_unittest.cc',
        '../../database/database_util_unittest.cc',
        '../../database/quota_table_unittest.cc',
        '../../dom_storage/dom_storage_area_unittest.cc',
        '../../dom_storage/dom_storage_context_unittest.cc',
        '../../dom_storage/dom_storage_database_unittest.cc',
        '../../dom_storage/dom_storage_map_unittest.cc',
        '../../dom_storage/session_storage_database_unittest.cc',
        '../../fileapi/file_system_database_test_helper.cc',
        '../../fileapi/file_system_database_test_helper.h',
        '../../fileapi/file_system_directory_database_unittest.cc',
        '../../fileapi/file_system_file_util_unittest.cc',
        '../../fileapi/file_system_mount_point_provider_unittest.cc',
        '../../fileapi/file_system_operation_unittest.cc',
        '../../fileapi/file_system_origin_database_unittest.cc',
        '../../fileapi/file_system_quota_client_unittest.cc',
        '../../fileapi/file_system_quota_unittest.cc',
        '../../fileapi/file_system_test_helper.cc',
        '../../fileapi/file_system_test_helper.h',
        '../../fileapi/file_system_usage_cache_unittest.cc',
        '../../fileapi/file_system_util_unittest.cc',
        '../../fileapi/isolated_context_unittest.cc',
        '../../fileapi/isolated_file_util_unittest.cc',
        '../../fileapi/local_file_util_unittest.cc',
        '../../fileapi/mock_file_system_options.cc',
        '../../fileapi/mock_file_system_options.h',
        '../../fileapi/obfuscated_file_util_unittest.cc',
        '../../fileapi/quota_file_util_unittest.cc',
        '../../fileapi/sandbox_mount_point_provider_unittest.cc',
        '../../fileapi/test_file_set.cc',
        '../../fileapi/test_file_set.h',
        '../../fileapi/webfilewriter_base_unittest.cc',
        '../../glue/bookmarklet_unittest.cc',
        '../../glue/cpp_bound_class_unittest.cc',
        '../../glue/cpp_variant_unittest.cc',
        '../../glue/dom_operations_unittest.cc',
        '../../glue/dom_serializer_unittest.cc',
        '../../glue/glue_serialize_unittest.cc',
        '../../glue/iframe_redirect_unittest.cc',
        '../../glue/mimetype_unittest.cc',
        '../../glue/multipart_response_delegate_unittest.cc',
        '../../glue/regular_expression_unittest.cc',
        '../../glue/resource_fetcher_unittest.cc',
        '../../glue/unittest_test_server.h',
        '../../glue/webcursor_unittest.cc',
        '../../glue/webframe_unittest.cc',
        '../../glue/webkit_glue_unittest.cc',
        '../../glue/webview_unittest.cc',
        '../../glue/worker_task_runner_unittest.cc',
        '../../media/buffered_data_source_unittest.cc',
        '../../media/buffered_resource_loader_unittest.cc',
        '../../media/skcanvas_video_renderer_unittest.cc',
        '../../media/test_response_generator.cc',
        '../../media/test_response_generator.h',
        '../../mocks/mock_resource_loader_bridge.h',
        '../../mocks/mock_webframeclient.h',
        '../../mocks/mock_weburlloader.cc',
        '../../mocks/mock_weburlloader.h',
        '../../plugins/npapi/plugin_group_unittest.cc',
        '../../plugins/npapi/plugin_lib_unittest.cc',
        '../../plugins/npapi/plugin_list_unittest.cc',
        '../../plugins/npapi/webplugin_impl_unittest.cc',
        '../../plugins/ppapi/host_var_tracker_unittest.cc',
        '../../plugins/ppapi/mock_plugin_delegate.cc',
        '../../plugins/ppapi/mock_plugin_delegate.h',
        '../../plugins/ppapi/mock_resource.h',
        '../../plugins/ppapi/ppapi_unittest.cc',
        '../../plugins/ppapi/ppapi_unittest.h',
        '../../plugins/ppapi/ppb_file_chooser_impl_unittest.cc',
        '../../plugins/ppapi/quota_file_io_unittest.cc',
        '../../plugins/ppapi/time_conversion_unittest.cc',
        '../../plugins/ppapi/url_request_info_unittest.cc',
        '../../quota/mock_quota_manager.cc',
        '../../quota/mock_quota_manager.h',
        '../../quota/mock_quota_manager_unittest.cc',
        '../../quota/mock_special_storage_policy.cc',
        '../../quota/mock_special_storage_policy.h',
        '../../quota/mock_storage_client.cc',
        '../../quota/mock_storage_client.h',
        '../../quota/quota_database_unittest.cc',
        '../../quota/quota_manager_unittest.cc',
        '../../quota/quota_temporary_storage_evictor_unittest.cc',
        '../webcore_unit_tests/BMPImageDecoder_unittest.cpp',
        '../webcore_unit_tests/ICOImageDecoder_unittest.cpp',
        'image_decoder_unittest.cc',
        'image_decoder_unittest.h',
        'media_leak_test.cc',
        'mock_spellcheck_unittest.cc',
        'plugin_tests.cc',
        'run_all_tests.cc',
        'test_shell_test.cc',
        'test_shell_test.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'sources': [ '<@(test_shell_windows_resource_files)' ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            'test_shell_pak',
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources!': [
             # TODO(port)
            '../../../skia/ext/platform_canvas_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'sources': [
            '../../chromeos/fileapi/file_access_permissions_unittest.cc',
            '../../chromeos/fileapi/memory_file_util.cc',
            '../../chromeos/fileapi/memory_file_util.h',
            '../../chromeos/fileapi/memory_file_util_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          # mac tests load the resources from the built test_shell beside the
          # test
          'dependencies': [
            'test_shell',
           ],
          'sources!': [
            # Disable the image decoder tests because we use CoreGraphics
            # code on mac and these tests are for the Skia image-decoders.
            '../webcore_unit_tests/BMPImageDecoder_unittest.cpp',
            '../webcore_unit_tests/ICOImageDecoder_unittest.cpp',
            '../webcore_unit_tests/XBMImageDecoder_unittest.cpp',
            'image_decoder_unittest.cc',
            'image_decoder_unittest.h',
          ],
          'sources': [
            '../../../skia/ext/skia_utils_mac_unittest.mm',
            '../../../skia/ext/bitmap_platform_device_mac_unittest.cc',
          ],
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
        }, {  # else: OS!=win
          'sources!': [
            '../../../skia/ext/vector_canvas_unittest.cc',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch!="arm"', {
      'targets': [
        {
          'target_name': 'npapi_test_common',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
          ],
          'sources': [
            '../../plugins/npapi/test/npapi_constants.cc',
            '../../plugins/npapi/test/npapi_constants.h',
            '../../plugins/npapi/test/plugin_client.cc',
            '../../plugins/npapi/test/plugin_client.h',
            '../../plugins/npapi/test/plugin_test.cc',
            '../../plugins/npapi/test/plugin_test.h',
            '../../plugins/npapi/test/plugin_test_factory.h',
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
            '../../plugins/npapi/test/npapi_test.cc',
            '../../plugins/npapi/test/npapi_test.def',
            '../../plugins/npapi/test/npapi_test.rc',
            '../../plugins/npapi/test/plugin_arguments_test.cc',
            '../../plugins/npapi/test/plugin_arguments_test.h',
            '../../plugins/npapi/test/plugin_create_instance_in_paint.cc',
            '../../plugins/npapi/test/plugin_create_instance_in_paint.h',
            '../../plugins/npapi/test/plugin_delete_plugin_in_deallocate_test.cc',
            '../../plugins/npapi/test/plugin_delete_plugin_in_deallocate_test.h',
            '../../plugins/npapi/test/plugin_delete_plugin_in_stream_test.cc',
            '../../plugins/npapi/test/plugin_delete_plugin_in_stream_test.h',
            '../../plugins/npapi/test/plugin_get_javascript_url_test.cc',
            '../../plugins/npapi/test/plugin_get_javascript_url_test.h',
            '../../plugins/npapi/test/plugin_get_javascript_url2_test.cc',
            '../../plugins/npapi/test/plugin_get_javascript_url2_test.h',
            '../../plugins/npapi/test/plugin_geturl_test.cc',
            '../../plugins/npapi/test/plugin_geturl_test.h',
            '../../plugins/npapi/test/plugin_javascript_open_popup.cc',
            '../../plugins/npapi/test/plugin_javascript_open_popup.h',
            '../../plugins/npapi/test/plugin_new_fails_test.cc',
            '../../plugins/npapi/test/plugin_new_fails_test.h',
            '../../plugins/npapi/test/plugin_npobject_identity_test.cc',
            '../../plugins/npapi/test/plugin_npobject_identity_test.h',
            '../../plugins/npapi/test/plugin_npobject_lifetime_test.cc',
            '../../plugins/npapi/test/plugin_npobject_lifetime_test.h',
            '../../plugins/npapi/test/plugin_npobject_proxy_test.cc',
            '../../plugins/npapi/test/plugin_npobject_proxy_test.h',
            '../../plugins/npapi/test/plugin_schedule_timer_test.cc',
            '../../plugins/npapi/test/plugin_schedule_timer_test.h',
            '../../plugins/npapi/test/plugin_setup_test.cc',
            '../../plugins/npapi/test/plugin_setup_test.h',
            '../../plugins/npapi/test/plugin_thread_async_call_test.cc',
            '../../plugins/npapi/test/plugin_thread_async_call_test.h',
            '../../plugins/npapi/test/plugin_windowed_test.cc',
            '../../plugins/npapi/test/plugin_windowed_test.h',
            '../../plugins/npapi/test/plugin_private_test.cc',
            '../../plugins/npapi/test/plugin_private_test.h',
            '../../plugins/npapi/test/plugin_test_factory.cc',
            '../../plugins/npapi/test/plugin_window_size_test.cc',
            '../../plugins/npapi/test/plugin_window_size_test.h',
            '../../plugins/npapi/test/plugin_windowless_test.cc',
            '../../plugins/npapi/test/plugin_windowless_test.h',
            '../../plugins/npapi/test/resource.h',
          ],
          'include_dirs': [
            '../../..',
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
                '../../plugins/npapi/test/plugin_npobject_lifetime_test.cc',
                 # The window APIs are necessarily platform-specific.
                '../../plugins/npapi/test/plugin_window_size_test.cc',
                '../../plugins/npapi/test/plugin_windowed_test.cc',
                 # Seems windows specific.
                '../../plugins/npapi/test/plugin_create_instance_in_paint.cc',
                '../../plugins/npapi/test/plugin_create_instance_in_paint.h',
                 # windows-specific resources
                '../../plugins/npapi/test/npapi_test.def',
                '../../plugins/npapi/test/npapi_test.rc',
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
            ['os_posix == 1 and OS != "mac"', {
              'sources!': [
                # Needs simple event record type porting
                '../../plugins/npapi/test/plugin_windowless_test.cc',
              ],
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
    ['os_posix == 1 and OS != "mac"', {
      'targets': [
        {
          'target_name': 'test_shell_resources',
          'type': 'none',
          'variables': {
            'grit_grd_file': './test_shell_resources.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/test_shell',
          },
          'actions': [
            {
              'action_name': 'test_shell_resources',
              'includes': [ '../../../build/grit_action.gypi' ],
            },
          ],
          'includes': [ '../../../build/grit_target.gypi' ],
        },
      ],
    }],
   ['OS=="win"', {
      'targets': [
        {
          # Helper application that disables ClearType during the
          # running of the layout tests
          'target_name': 'layout_test_helper',
          'type': 'executable',
          'variables': {
            'chromium_code': 1,
          },
          'sources': [
            'win/layout_test_helper.cc',
          ],
        },
      ],
    }],
   ['OS=="mac"', {
      'targets': [
        {
          # Helper application that manages the color sync profile on mac
          # for the test shells run by the layout tests.
          'target_name': 'layout_test_helper',
          'type': 'executable',
          'variables': {
            'chromium_code': 1,
          },
          'sources': [
            'mac/layout_test_helper.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
        },
      ],
    }],
  ],
}

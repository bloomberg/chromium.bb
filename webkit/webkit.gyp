# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/WebKit/WebKit/chromium/features.gypi',
    '../third_party/WebKit/WebCore/WebCore.gypi',
  ],
  'variables': {
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '../chrome/tools/build/apply_locales.py',],

    # We can't turn on warnings on Windows and Linux until we upstream the
    # WebKit API.
    'conditions': [
      ['OS=="mac"', {
        'chromium_code': 1,
      }],
    ],

    # List of DevTools source files, ordered by dependencies. It is used both
    # for copying them to resource dir, and for generating 'devtools.html' file.
    'devtools_files': [
      'glue/devtools/js/devtools.css',
      'glue/devtools/js/base.js',
      'glue/devtools/js/inspector_controller_impl.js',
      'glue/devtools/js/debugger_agent.js',
      '../v8/tools/codemap.js',
      '../v8/tools/consarray.js',
      '../v8/tools/csvparser.js',
      '../v8/tools/logreader.js',
      '../v8/tools/profile.js',
      '../v8/tools/profile_view.js',
      '../v8/tools/splaytree.js',
      'glue/devtools/js/profiler_processor.js',
      'glue/devtools/js/heap_profiler_panel.js',
      'glue/devtools/js/devtools.js',
      'glue/devtools/js/devtools_host_stub.js',
      'glue/devtools/js/tests.js',
    ],

    'debug_devtools%': 0,
  },
  'targets': [
    {
      # Currently, builders assume webkit.sln builds test_shell on windows.
      # We should change this, but for now allows trybot runs.
      # for now.
      'target_name': 'pull_in_test_shell',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'tools/test_shell/test_shell.gyp:*',
          ],
        }],
      ],
    },
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'msvs_guid': '0B469837-3D46-484A-AFB3-C5A6C68730B9',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'input_path': 'glue/webkit_resources.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_resources.h',
            '<(grit_out_dir)/webkit_resources.pak',
            '<(grit_out_dir)/webkit_resources.rc',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'msvs_guid': '60B43839-95E6-4526-A661-209F16335E0E',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'input_path': 'glue/webkit_strings.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_strings.h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/webkit_strings_ZZLOCALE.pak\' <(locales))',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'appcache',
      'type': '<(library)',
      'msvs_guid': '0B945915-31A7-4A07-A5B5-568D737A39B1',
      'dependencies': [
        '../net/net.gyp:net',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        # This list contains all .h and .cc in appcache except for test code.
        'appcache/appcache.cc',
        'appcache/appcache.h',
        'appcache/appcache_backend_impl.cc',
        'appcache/appcache_backend_impl.h',
        'appcache/appcache_entry.h',
        'appcache/appcache_frontend_impl.cc',
        'appcache/appcache_frontend_impl.h',
        'appcache/appcache_group.cc',
        'appcache/appcache_group.h',
        'appcache/appcache_host.cc',
        'appcache/appcache_host.h',
        'appcache/appcache_interceptor.cc',
        'appcache/appcache_interceptor.h',
        'appcache/appcache_interfaces.cc',
        'appcache/appcache_interfaces.h',
        'appcache/appcache_request_handler.cc',
        'appcache/appcache_request_handler.h',
        'appcache/appcache_response.cc',
        'appcache/appcache_response.h',
        'appcache/appcache_service.cc',
        'appcache/appcache_service.h',
        'appcache/appcache_storage.cc',
        'appcache/appcache_storage.h',
        'appcache/appcache_storage_impl.cc',
        'appcache/appcache_storage_impl.h',
        'appcache/appcache_thread.cc',
        'appcache/appcache_thread.h',
        'appcache/appcache_working_set.cc',
        'appcache/appcache_working_set.h',
        'appcache/appcache_update_job.cc',
        'appcache/appcache_update_job.h',
        'appcache/appcache_url_request_job.cc',
        'appcache/appcache_url_request_job.h',
        'appcache/manifest_parser.cc',
        'appcache/manifest_parser.h',
        'appcache/mock_appcache_storage.cc',
        'appcache/mock_appcache_storage.h',
        'appcache/web_application_cache_host_impl.cc',
        'appcache/web_application_cache_host_impl.h',
      ],
    },
    {
      'target_name': 'database',
      'type': '<(library)',
      'msvs_guid': '1DA00DDD-44E5-4C56-B2CC-414FB0164492',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'sources': [
        'database/databases_table.cc',
        'database/databases_table.h',
        'database/database_tracker.cc',
        'database/database_tracker.h',
        'database/database_util.cc',
        'database/database_util.h',
        'database/vfs_backend.cc',
        'database/vfs_backend.h',
      ],
    },
    {
      'target_name': 'glue',
      'type': '<(library)',
      'msvs_guid': 'C66B126D-0ECE-4CA2-B6DC-FA780AFBBF09',
      'dependencies': [
        '../app/app.gyp:app_base',
        '../net/net.gyp:net',
        'inspector_resources',
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
        'webkit_resources',
        'webkit_strings',
      ],
      'actions': [
        {
          'action_name': 'webkit_version',
          'inputs': [
            'build/webkit_version.py',
            '../third_party/WebKit/WebCore/Configurations/Version.xcconfig',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/webkit_version.h',
          ],
          'action': ['python', '<@(_inputs)', '<(INTERMEDIATE_DIR)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        'glue/devtools/apu_agent_delegate.h',
        'glue/devtools/devtools_rpc.h',
        'glue/devtools/devtools_rpc_js.h',
        'glue/devtools/bound_object.cc',
        'glue/devtools/bound_object.h',
        'glue/devtools/debugger_agent.h',
        'glue/devtools/debugger_agent_impl.cc',
        'glue/devtools/debugger_agent_impl.h',
        'glue/devtools/debugger_agent_manager.cc',
        'glue/devtools/debugger_agent_manager.h',
        'glue/devtools/tools_agent.h',
        'glue/media/buffered_data_source.cc',
        'glue/media/buffered_data_source.h',
        'glue/media/media_resource_loader_bridge_factory.cc',
        'glue/media/media_resource_loader_bridge_factory.h',
        'glue/media/simple_data_source.cc',
        'glue/media/simple_data_source.h',
        'glue/media/video_renderer_impl.cc',
        'glue/media/video_renderer_impl.h',
        'glue/plugins/nphostapi.h',
        'glue/plugins/fake_plugin_window_tracker_mac.h',
        'glue/plugins/fake_plugin_window_tracker_mac.cc',
        'glue/plugins/gtk_plugin_container.h',
        'glue/plugins/gtk_plugin_container.cc',
        'glue/plugins/gtk_plugin_container_manager.h',
        'glue/plugins/gtk_plugin_container_manager.cc',
        'glue/plugins/npapi_extension_thunk.cc',
        'glue/plugins/npapi_extension_thunk.h',
        'glue/plugins/plugin_constants_win.h',
        'glue/plugins/plugin_host.cc',
        'glue/plugins/plugin_host.h',
        'glue/plugins/plugin_instance.cc',
        'glue/plugins/plugin_instance.h',
        'glue/plugins/plugin_lib.cc',
        'glue/plugins/plugin_lib.h',
        'glue/plugins/plugin_lib_linux.cc',
        'glue/plugins/plugin_lib_mac.mm',
        'glue/plugins/plugin_lib_win.cc',
        'glue/plugins/plugin_list.cc',
        'glue/plugins/plugin_list.h',
        'glue/plugins/plugin_list_linux.cc',
        'glue/plugins/plugin_list_mac.mm',
        'glue/plugins/plugin_list_win.cc',
        'glue/plugins/plugin_stream.cc',
        'glue/plugins/plugin_stream.h',
        'glue/plugins/plugin_stream_posix.cc',
        'glue/plugins/plugin_stream_url.cc',
        'glue/plugins/plugin_stream_url.h',
        'glue/plugins/plugin_stream_win.cc',
        'glue/plugins/plugin_string_stream.cc',
        'glue/plugins/plugin_string_stream.h',
        'glue/plugins/plugin_stubs.cc',
        'glue/plugins/webplugin_2d_device_delegate.h',
        'glue/plugins/webplugin_3d_device_delegate.h',
        'glue/plugins/webplugin_delegate_impl.cc',
        'glue/plugins/webplugin_delegate_impl.h',
        'glue/plugins/webplugin_delegate_impl_gtk.cc',
        'glue/plugins/webplugin_delegate_impl_mac.mm',
        'glue/plugins/webplugin_delegate_impl_win.cc',
        'glue/alt_error_page_resource_fetcher.cc',
        'glue/alt_error_page_resource_fetcher.h',
        'glue/context_menu.h',
        'glue/cpp_binding_example.cc',
        'glue/cpp_binding_example.h',
        'glue/cpp_bound_class.cc',
        'glue/cpp_bound_class.h',
        'glue/cpp_variant.cc',
        'glue/cpp_variant.h',
        'glue/dom_operations.cc',
        'glue/dom_operations.h',
        'glue/dom_operations_private.h',
        'glue/dom_serializer.cc',
        'glue/dom_serializer.h',
        'glue/dom_serializer_delegate.h',
        'glue/entity_map.cc',
        'glue/entity_map.h',
        'glue/form_data.h',
        'glue/form_field.cc',
        'glue/form_field.h',
        'glue/form_field_values.cc',
        'glue/form_field_values.h',
        'glue/ftp_directory_listing_response_delegate.cc',
        'glue/ftp_directory_listing_response_delegate.h',
        'glue/glue_serialize.cc',
        'glue/glue_serialize.h',
        'glue/glue_util.cc',
        'glue/glue_util.h',
        'glue/image_decoder.cc',
        'glue/image_decoder.h',
        'glue/image_resource_fetcher.cc',
        'glue/image_resource_fetcher.h',
        'glue/multipart_response_delegate.cc',
        'glue/multipart_response_delegate.h',
        'glue/npruntime_util.cc',
        'glue/npruntime_util.h',
        'glue/password_form.h',
        'glue/password_form_dom_manager.cc',
        'glue/password_form_dom_manager.h',
        'glue/resource_fetcher.cc',
        'glue/resource_fetcher.h',
        'glue/resource_loader_bridge.cc',
        'glue/resource_loader_bridge.h',
        'glue/resource_type.h',
        'glue/scoped_clipboard_writer_glue.h',
        'glue/simple_webmimeregistry_impl.cc',
        'glue/simple_webmimeregistry_impl.h',
        'glue/webaccessibility.cc',
        'glue/webaccessibility.h',
        'glue/webclipboard_impl.cc',
        'glue/webclipboard_impl.h',
        'glue/webcookie.h',
        'glue/webcursor.cc',
        'glue/webcursor.h',
        'glue/webcursor_gtk.cc',
        'glue/webcursor_gtk_data.h',
        'glue/webcursor_mac.mm',
        'glue/webcursor_win.cc',
        'glue/webdevtoolsagent_impl.cc',
        'glue/webdevtoolsagent_impl.h',
        'glue/webdevtoolsfrontend_impl.cc',
        'glue/webdevtoolsfrontend_impl.h',
        'glue/webdropdata.cc',
        'glue/webdropdata_win.cc',
        'glue/webdropdata.h',
        'glue/webkit_glue.cc',
        'glue/webkit_glue.h',
        'glue/webkitclient_impl.cc',
        'glue/webkitclient_impl.h',
        'glue/webmediaplayer_impl.h',
        'glue/webmediaplayer_impl.cc',
        'glue/webmenuitem.h',
        'glue/webmenurunner_mac.h',
        'glue/webmenurunner_mac.mm',
        'glue/webpasswordautocompletelistener_impl.cc',
        'glue/webpasswordautocompletelistener_impl.h',
        'glue/webplugin.h',
        'glue/webplugin_delegate.h',
        'glue/webplugin_impl.cc',
        'glue/webplugin_impl.h',
        'glue/webplugininfo.h',
        'glue/webpreferences.cc',
        'glue/webpreferences.h',
        'glue/websocketstreamhandle_bridge.h',
        'glue/websocketstreamhandle_delegate.h',
        'glue/websocketstreamhandle_impl.cc',
        'glue/websocketstreamhandle_impl.h',
        'glue/webthemeengine_impl_win.cc',
        'glue/weburlloader_impl.cc',
        'glue/weburlloader_impl.h',
        'glue/window_open_disposition.h',
        'glue/window_open_disposition.cc',

        # These files used to be built in the webcore target, but moved here
        # since part of glue.
        'extensions/v8/benchmarking_extension.cc',
        'extensions/v8/benchmarking_extension.h',
        'extensions/v8/gc_extension.cc',
        'extensions/v8/gc_extension.h',
        'extensions/v8/gears_extension.cc',
        'extensions/v8/gears_extension.h',
        'extensions/v8/heap_profiler_extension.cc',
        'extensions/v8/heap_profiler_extension.h',
        'extensions/v8/interval_extension.cc',
        'extensions/v8/interval_extension.h',
        'extensions/v8/playback_extension.cc',
        'extensions/v8/playback_extension.h',
        'extensions/v8/profiler_extension.cc',
        'extensions/v8/profiler_extension.h',

      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../base/base.gyp:linux_versioninfo',
          ],
          'export_dependent_settings': [
            # Users of webcursor.h need the GTK include path.
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }, { # else: OS!="linux" and OS!="freebsd"
          'sources/': [['exclude', '_(linux|gtk)(_data)?\\.cc$'],
                       ['exclude', r'/gtk_']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']]
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            # These files are Windows-only now but may be ported to other
            # platforms.
            'glue/webaccessibility.cc',
            'glue/webaccessibility.h',
            'glue/webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '../chrome/third_party/wtl/include',
          ],
          'dependencies': [
            '../build/win/system.gyp:cygwin',
            'default_plugin/default_plugin.gyp:default_plugin',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'inspector_resources',
      'type': 'none',
      'msvs_guid': '5330F8EE-00F5-D65C-166E-E3150171055D',
      'dependencies': [
        'devtools_html',
      ],
      'conditions': [
        ['debug_devtools==0', {
          'dependencies+': [
            'concatenated_devtools_js',
          ],
        }],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector',
          'files': [
            '<@(devtools_files)',
            '<@(webinspector_files)',
          ],
          'conditions': [
            ['debug_devtools==0', {
              'files/': [['exclude', '\\.js$']],
            }],
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
          'files': [

            '<@(webinspector_image_files)',

            'glue/devtools/js/Images/segmentChromium.png',
            'glue/devtools/js/Images/segmentHoverChromium.png',
            'glue/devtools/js/Images/segmentHoverEndChromium.png',
            'glue/devtools/js/Images/segmentSelectedChromium.png',
            'glue/devtools/js/Images/segmentSelectedEndChromium.png',
            'glue/devtools/js/Images/statusbarBackgroundChromium.png',
            'glue/devtools/js/Images/statusbarBottomBackgroundChromium.png',
            'glue/devtools/js/Images/statusbarButtonsChromium.png',
            'glue/devtools/js/Images/statusbarMenuButtonChromium.png',
            'glue/devtools/js/Images/statusbarMenuButtonSelectedChromium.png',
          ],
        },
      ],
    },
    {
      'target_name': 'devtools_html',
      'type': 'none',
      'msvs_guid': '9BE5D4D5-E800-44F9-B6C0-27DF15A9D817',
      'sources': [
        '<(PRODUCT_DIR)/resources/inspector/devtools.html',
      ],
      'actions': [
        {
          'action_name': 'devtools_html',
          'inputs': [
            'build/generate_devtools_html.py',
            # See issue 29695: webkit.gyp is a source file for devtools.html.
            'webkit.gyp',
            '../third_party/WebKit/WebCore/inspector/front-end/inspector.html',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
          ],
          'action': ['python', '<@(_inputs)', '<@(_outputs)', '<@(devtools_files)'],
        },
      ],
    },
    {
      'target_name': 'concatenated_devtools_js',
      'type': 'none',
      'msvs_guid': '8CCFDF4A-B702-4988-9207-623D1477D3E7',
      'dependencies': [
        'devtools_html',
      ],
      'sources': [
        '<(PRODUCT_DIR)/resources/inspector/devtools.js',
      ],
      'actions': [
        {
          'action_name': 'concatenate_devtools_js',
          'inputs': [
            'build/concatenate_js_files.py',
            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
            '<@(webinspector_files)',
            '<@(devtools_files)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/resources/inspector/devtools.js',
          ],
          'action': ['python', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

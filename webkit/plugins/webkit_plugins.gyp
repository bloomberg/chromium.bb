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
      'target_name': 'test_mock_plugin_list',
      'type': 'static_library',
      'dependencies': [
        'plugins_common'
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'npapi/mock_plugin_list.cc',
        'npapi/mock_plugin_list.h',
      ]
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
            'npapi/test/npapi_constants.cc',
            'npapi/test/npapi_constants.h',
            'npapi/test/plugin_client.cc',
            'npapi/test/plugin_client.h',
            'npapi/test/plugin_test.cc',
            'npapi/test/plugin_test.h',
            'npapi/test/plugin_test_factory.h',
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
            'npapi/test/npapi_test.cc',
            'npapi/test/npapi_test.def',
            'npapi/test/npapi_test.rc',
            'npapi/test/plugin_arguments_test.cc',
            'npapi/test/plugin_arguments_test.h',
            'npapi/test/plugin_create_instance_in_paint.cc',
            'npapi/test/plugin_create_instance_in_paint.h',
            'npapi/test/plugin_delete_plugin_in_deallocate_test.cc',
            'npapi/test/plugin_delete_plugin_in_deallocate_test.h',
            'npapi/test/plugin_delete_plugin_in_stream_test.cc',
            'npapi/test/plugin_delete_plugin_in_stream_test.h',
            'npapi/test/plugin_execute_stream_javascript.cc',
            'npapi/test/plugin_execute_stream_javascript.h',
            'npapi/test/plugin_get_javascript_url_test.cc',
            'npapi/test/plugin_get_javascript_url_test.h',
            'npapi/test/plugin_get_javascript_url2_test.cc',
            'npapi/test/plugin_get_javascript_url2_test.h',
            'npapi/test/plugin_geturl_test.cc',
            'npapi/test/plugin_geturl_test.h',
            'npapi/test/plugin_javascript_open_popup.cc',
            'npapi/test/plugin_javascript_open_popup.h',
            'npapi/test/plugin_new_fails_test.cc',
            'npapi/test/plugin_new_fails_test.h',
            'npapi/test/plugin_npobject_identity_test.cc',
            'npapi/test/plugin_npobject_identity_test.h',
            'npapi/test/plugin_npobject_lifetime_test.cc',
            'npapi/test/plugin_npobject_lifetime_test.h',
            'npapi/test/plugin_npobject_proxy_test.cc',
            'npapi/test/plugin_npobject_proxy_test.h',
            'npapi/test/plugin_schedule_timer_test.cc',
            'npapi/test/plugin_schedule_timer_test.h',
            'npapi/test/plugin_setup_test.cc',
            'npapi/test/plugin_setup_test.h',
            'npapi/test/plugin_thread_async_call_test.cc',
            'npapi/test/plugin_thread_async_call_test.h',
            'npapi/test/plugin_windowed_test.cc',
            'npapi/test/plugin_windowed_test.h',
            'npapi/test/plugin_private_test.cc',
            'npapi/test/plugin_private_test.h',
            'npapi/test/plugin_test_factory.cc',
            'npapi/test/plugin_window_size_test.cc',
            'npapi/test/plugin_window_size_test.h',
            'npapi/test/plugin_windowless_test.cc',
            'npapi/test/plugin_windowless_test.h',
            'npapi/test/resource.h',
          ],
          'include_dirs': [
            '../..',
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
                'npapi/test/plugin_npobject_lifetime_test.cc',
                 # The window APIs are necessarily platform-specific.
                'npapi/test/plugin_window_size_test.cc',
                'npapi/test/plugin_windowed_test.cc',
                 # Seems windows specific.
                'npapi/test/plugin_create_instance_in_paint.cc',
                'npapi/test/plugin_create_instance_in_paint.h',
                 # windows-specific resources
                'npapi/test/npapi_test.def',
                'npapi/test/npapi_test.rc',
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


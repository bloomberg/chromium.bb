# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ppapi_tests',
      'type': 'loadable_module',
      'include_dirs': [
        'lib/gl/include',
      ],
      'defines': [
        'GL_GLEXT_PROTOTYPES',
      ],
      'sources': [
        '<@(test_common_source_files)',
        '<@(test_trusted_source_files)',
      ],
      'dependencies': [
        'ppapi.gyp:ppapi_cpp',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            # Keep 'test_case.html.mock-http-headers' with 'test_case.html'.
            'tests/test_case.html',
            'tests/test_case.html.mock-http-headers',
            'tests/test_page.css',
            'tests/ppapi_nacl_tests_newlib.nmf',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/test_url_loader_data',
          'files': [
            'tests/test_url_loader_data/hello.txt',
          ],
        },
      ],
      'run_as': {
        'action': [
          '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
          '--enable-pepper-testing',
          '--register-pepper-plugins=$(TargetPath);application/x-ppapi-tests',
          'file://$(ProjectDir)/tests/test_case.html?testcase=',
        ],
      },
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
      'target_name': 'ppapi_unittest_shared',
      'type': 'static_library',
      'dependencies': [
        'ppapi_proxy',
        'ppapi_shared',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:test_support_ipc',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'proxy/ppapi_proxy_test.cc',
        'proxy/ppapi_proxy_test.h',
        'proxy/resource_message_test_sink.cc',
        'proxy/resource_message_test_sink.h',
        'shared_impl/test_globals.cc',
        'shared_impl/test_globals.h',
      ],
    },

    {
      'target_name': 'ppapi_perftests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'ppapi_proxy',
        'ppapi_shared',
        'ppapi_unittest_shared',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'proxy/ppapi_perftests.cc',
        'proxy/ppp_messaging_proxy_perftest.cc',
      ],
    },
    {
      'target_name': 'ppapi_unittests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'ppapi_host',
        'ppapi_proxy',
        'ppapi_shared',
        'ppapi_unittest_shared',
        '../base/base.gyp:test_support_base',
        '../gpu/gpu.gyp:gpu_ipc',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:test_support_ipc',
        '../media/media.gyp:shared_memory_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/surface/surface.gyp:surface',
      ],
      'sources': [
        'proxy/run_all_unittests.cc',

        'host/resource_message_filter_unittest.cc',
        'proxy/device_enumeration_resource_helper_unittest.cc',
        'proxy/file_chooser_resource_unittest.cc',
        'proxy/flash_resource_unittest.cc',
        'proxy/mock_resource.cc',
        'proxy/mock_resource.h',
        'proxy/plugin_dispatcher_unittest.cc',
        'proxy/plugin_resource_tracker_unittest.cc',
        'proxy/plugin_var_tracker_unittest.cc',
        'proxy/ppb_var_unittest.cc',
        'proxy/ppp_instance_private_proxy_unittest.cc',
        'proxy/ppp_instance_proxy_unittest.cc',
        'proxy/ppp_messaging_proxy_unittest.cc',
        'proxy/printing_resource_unittest.cc',
        'proxy/serialized_var_unittest.cc',
        'proxy/websocket_resource_unittest.cc',
        'shared_impl/resource_tracker_unittest.cc',
        'shared_impl/thread_aware_callback_unittest.cc',
        'shared_impl/time_conversion_unittest.cc',
        'shared_impl/tracked_callback_unittest.cc',
        'shared_impl/var_tracker_unittest.cc',
      ],
      'conditions': [
        [ 'os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            [ 'linux_use_tcmalloc == 1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'ppapi_example_skeleton',
      'suppress_wildcard': 1,
      'type': 'none',
      'direct_dependent_settings': {
        'product_name': '>(_target_name)',
        'conditions': [
          ['os_posix==1 and OS!="mac"', {
            'cflags': ['-fvisibility=hidden'],
            'type': 'shared_library',
            # -gstabs, used in the official builds, causes an ICE. Simply remove
            # it.
            'cflags!': ['-gstabs'],
          }],
          ['OS=="win"', {
            'type': 'shared_library',
          }],
          ['OS=="mac"', {
            'type': 'loadable_module',
            'mac_bundle': 1,
            'product_extension': 'plugin',
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                # Not to strip important symbols by -Wl,-dead_strip.
                '-Wl,-exported_symbol,_PPP_GetInterface',
                '-Wl,-exported_symbol,_PPP_InitializeModule',
                '-Wl,-exported_symbol,_PPP_ShutdownModule'
              ]},
          }],
        ],
      },
    },
    {
      'target_name': 'ppapi_example_mouse_cursor',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/mouse_cursor/mouse_cursor.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_mouse_lock',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/mouse_lock/mouse_lock.cc',
      ],
    },

    {
      'target_name': 'ppapi_example_gamepad',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/gamepad/gamepad.cc',
      ],
    },

    {
      'target_name': 'ppapi_example_c_stub',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_c',
      ],
      'sources': [
        'examples/stub/stub.c',
      ],
    },
    {
      'target_name': 'ppapi_example_cc_stub',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/stub/stub.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_audio',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/audio/audio.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_audio_input',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/audio_input/audio_input.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_file_chooser',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/file_chooser/file_chooser.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_graphics_2d',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_c',
      ],
      'sources': [
        'examples/2d/graphics_2d_example.c',
      ],
    },
    {
      'target_name': 'ppapi_example_ime',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/ime/ime.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_paint_manager',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/2d/paint_manager_example.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_input',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/input/pointer_event_input.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_post_message',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/scripting/post_message.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_scroll',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/2d/scroll.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_simple_font',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/font/simple_font.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_url_loader',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/url_loader/streaming.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_url_loader_file',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/url_loader/stream_to_file.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_gles2',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
        'ppapi.gyp:ppapi_egl',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/gles2/gles2.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_video_decode',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
        'ppapi.gyp:ppapi_egl',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/video_decode/video_decode.cc',
        'examples/video_decode/testdata.h',
      ],
    },
    {
      'target_name': 'ppapi_example_vc',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
        'ppapi.gyp:ppapi_egl',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/video_capture/video_capture.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_enumerate_devices',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/enumerate_devices/enumerate_devices.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_flash_topmost',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/flash_topmost/flash_topmost.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_printing',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/printing/printing.cc',
      ],
    },
  ],
}

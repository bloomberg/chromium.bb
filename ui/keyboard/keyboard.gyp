# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'keyboard_mojom_gen_js': '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/webui/keyboard.mojom.js',
  },
  'targets': [
    {
      'target_name': 'keyboard_mojom_bindings',
      'type': 'none',
      'sources': [
        'webui/keyboard.mojom',
      ],
      'includes': [ '../../mojo/public/tools/bindings/mojom_bindings_generator.gypi' ],
    },
    {
      # GN version: //ui/keyboard:resources
      'target_name': 'keyboard_resources',
      'dependencies': [ 'keyboard_mojom_bindings', ],
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard',
      },
      'actions': [
        {
          'action_name': 'keyboard_resources',
          'variables': {
            'grit_grd_file': 'keyboard_resources.grd',
            'grit_additional_defines': [
              '-E', 'keyboard_mojom_gen_js=<(keyboard_mojom_gen_js)',
            ],
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/keyboard_resources.pak',
          ],
        },
      ],
    },
    {
      # GN version: //ui/keyboard
      'target_name': 'keyboard',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../content/content.gyp:content_browser',
        '../../ipc/ipc.gyp:ipc',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/mojo_base.gyp:mojo_js_bindings',
        '../../mojo/mojo_base.gyp:mojo_system_impl',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../aura/aura.gyp:aura',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../wm/wm.gyp:wm',
        'keyboard_mojom_bindings',
        'keyboard_resources',
      ],
      'defines': [
        'KEYBOARD_IMPLEMENTATION',
      ],
      'sources': [
        'keyboard.cc',
        'keyboard.h',
        'keyboard_constants.cc',
        'keyboard_constants.h',
        'keyboard_controller.cc',
        'keyboard_controller.h',
        'keyboard_controller_observer.h',
        'keyboard_controller_proxy.cc',
        'keyboard_controller_proxy.h',
        'keyboard_layout_manager.h',
        'keyboard_layout_manager.cc',
        'keyboard_export.h',
        'keyboard_switches.cc',
        'keyboard_switches.h',
        'keyboard_util.cc',
        'keyboard_util.h',
        'webui/vk_mojo_handler.cc',
        'webui/vk_mojo_handler.h',
        'webui/vk_webui_controller.cc',
        'webui/vk_webui_controller.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/webui/keyboard.mojom.cc',
      ]
    },
    {
      'target_name': 'keyboard_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../content/content.gyp:content',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../url/url.gyp:url_lib',
        '../aura/aura.gyp:aura',
        '../aura/aura.gyp:aura_test_support',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../compositor/compositor.gyp:compositor_test_support',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../resources/ui_resources.gyp:ui_test_pak',
        '../wm/wm.gyp:wm',
        'keyboard',
      ],
      'sources': [
        'test/run_all_unittests.cc',
        'keyboard_controller_unittest.cc',
      ],
      'conditions': [
        ['OS=="linux" and use_allocator!="none"', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
          'link_settings': {
            'ldflags': ['-rdynamic'],
          },
        }],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
  ],
}

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_test_support',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'base/test/cocoa_test_event_utils.h',
        'base/test/cocoa_test_event_utils.mm',
        'base/test/ui_cocoa_test_helper.h',
        'base/test/ui_cocoa_test_helper.mm',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['OS=="mac"', {
          'type': 'static_library',
        }, { # OS != "mac"
          'type': 'none',
        }],
      ],
    },
    {
      'target_name': 'ui_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'gfx_resources',
        'ui',
        'ui_test_support',
      ],
      'sources': [
        'base/animation/animation_container_unittest.cc',
        'base/animation/animation_unittest.cc',
        'base/animation/multi_animation_unittest.cc',
        'base/animation/slide_animation_unittest.cc',
        'base/clipboard/clipboard_unittest.cc',
        'base/clipboard/custom_data_helper_unittest.cc',
        'base/cocoa/base_view_unittest.mm',
        'base/gtk/gtk_expanded_container_unittest.cc',
        'base/gtk/gtk_im_context_util_unittest.cc',
        'base/ime/character_composer_unittest.cc',
        'base/l10n/l10n_util_mac_unittest.mm',
        'base/l10n/l10n_util_unittest.cc',
        'base/models/tree_node_iterator_unittest.cc',
        'base/models/tree_node_model_unittest.cc',
        'base/range/range_unittest.cc',
        'base/range/range_mac_unittest.mm',
        'base/range/range_win_unittest.cc',
        'base/resource/data_pack_literal.cc',
        'base/resource/data_pack_unittest.cc',
        'base/resource/resource_bundle_unittest.cc',
        'base/text/bytes_formatting_unittest.cc',
        'base/test/data/resource.h',
        'base/text/text_elider_unittest.cc',
        'gfx/blit_unittest.cc',
        'gfx/codec/jpeg_codec_unittest.cc',
        'gfx/codec/png_codec_unittest.cc',
        'gfx/color_analysis_unittest.cc',
        'gfx/color_utils_unittest.cc',
        'gfx/font_unittest.cc',
        'gfx/image/image_mac_unittest.mm',
        'gfx/image/image_unittest.cc',
        'gfx/image/image_unittest_util.h',
        'gfx/image/image_unittest_util.cc',
        'gfx/insets_unittest.cc',
        'gfx/rect_unittest.cc',
        'gfx/run_all_unittests.cc',
        'gfx/screen_unittest.cc',
        'gfx/skbitmap_operations_unittest.cc',
        'gfx/skia_util_unittest.cc',
        'gfx/test_suite.cc',
        'gfx/test_suite.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['OS == "win"', {
          'sources': [
            'base/dragdrop/os_exchange_data_win_unittest.cc',
            'base/view_prop_unittest.cc',
            # TODO(brettw) re-enable this when the dependencies on WindowImpl are fixed!
            'gfx/icon_util_unittest.cc',
            'gfx/native_theme_win_unittest.cc',
          ],
          'include_dirs': [
            '../..',
            '../third_party/wtl/include',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'd2d1.dll',
                'd3d10_1.dll',
              ],
              'AdditionalDependencies': [
                'd2d1.lib',
                'd3d10_1.lib',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-limm32.lib',
              '-loleacc.lib',
            ],
          },
        }],
        ['OS == "linux" and toolkit_views==1', {
          'sources': [
            'base/x/events_x_unittest.cc',
          ],
        }],
        ['OS != "mac"', {
          'sources': [
            'gfx/transform_unittest.cc',
            'gfx/interpolated_transform_unittest.cc',
          ],
        }],
        ['use_glib == 1', {
          'dependencies': [
            '../build/linux/system.gyp:pangocairo',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
            'base/strings/ui_strings.gyp:ui_unittest_strings',
          ],
          'conditions': [
            ['linux_use_tcmalloc==1', {
               'dependencies': [
                 '../base/allocator/allocator.gyp:allocator',
               ],
            }],
            ['toolkit_views==1', {
              'sources!': [
                'browser/ui/gtk/gtk_expanded_container_unittest.cc',
              ],
            }],
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'sources': [
            'base/dragdrop/gtk_dnd_util_unittest.cc',
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['toolkit_views==1', {
          'sources': [
            'gfx/render_text_unittest.cc',
          ],
        }],
        ['use_aura==1', {
          'sources!': [
            'base/view_prop_unittest.cc',
            'gfx/screen_unittest.cc',
          ],
        }],
        ['use_ibus != 1', {
          'sources/': [
            ['exclude', 'base/ime/character_composer_unittest.cc'],
          ],
        }],
      ],
    },
  ],
}

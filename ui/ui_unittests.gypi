# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_unittests',
      'type': 'executable',
      'msvs_guid': 'C412B00F-2098-4833-B3DE-A1B8B7A094F0',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'ui_base',
        'ui_gfx',
        'gfx_resources',
      ],
      'sources': [
        'base/animation/animation_container_unittest.cc',
        'base/animation/animation_unittest.cc',
        'base/animation/multi_animation_unittest.cc',
        'base/animation/slide_animation_unittest.cc',
        'base/clipboard/clipboard_unittest.cc',
        'base/gtk/gtk_im_context_util_unittest.cc',
        'base/range/range_unittest.cc',
        'base/range/range_unittest.mm',
        'gfx/blit_unittest.cc',
        'gfx/codec/jpeg_codec_unittest.cc',
        'gfx/codec/png_codec_unittest.cc',
        'gfx/color_utils_unittest.cc',
        'gfx/font_unittest.cc',
        'gfx/image_mac_unittest.mm',
        'gfx/image_unittest.cc',
        'gfx/image_unittest_util.h',
        'gfx/image_unittest_util.cc',
        'gfx/insets_unittest.cc',
        'gfx/rect_unittest.cc',
        'gfx/run_all_unittests.cc',
        'gfx/skbitmap_operations_unittest.cc',
        'gfx/test_suite.cc',
        'gfx/test_suite.h',
        'views/rendering/border_unittest.cc',
        'views/view_unittest.cc',
        'views/widget/native_widget_win_unittest.cc',
        'views/widget/root_view_unittest.cc',
        'views/widget/widget_test_util.cc',
        'views/widget/widget_test_util.h',
        'views/widget/widget_unittest.cc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['toolkit_views2==1', {
          'dependencies': [
            'v2',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            # TODO(brettw) re-enable this when the dependencies on WindowImpl are fixed!
            'gfx/canvas_direct2d_unittest.cc',
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
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../app/app.gyp:app_base',
            '../build/linux/system.gyp:gtk',
          ],
          'conditions': [
            ['linux_use_tcmalloc==1', {
               'dependencies': [
                 '../base/allocator/allocator.gyp:allocator',
               ],
            }],
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

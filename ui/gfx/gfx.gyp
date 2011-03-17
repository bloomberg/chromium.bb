# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_info_cmd': ['python', '../../tools/grit/grit_info.py',
                      '<@(grit_defines)'],
    'grit_cmd': ['python', '../../tools/grit/grit.py'],
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/gfx',
  },
  'targets': [
    {
      'target_name': 'gfx_unittests',
      'type': 'executable',
      'msvs_guid': 'C8BD2821-EAE5-4AC6-A0E4-F82CAC2956CC',
      'dependencies': [
        'gfx',
        'gfx_resources',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'blit_unittest.cc',
        'codec/jpeg_codec_unittest.cc',
        'codec/png_codec_unittest.cc',
        'color_utils_unittest.cc',
        'font_unittest.cc',
        'image_unittest.cc',
        'image_unittest.h',
        'insets_unittest.cc',
        'rect_unittest.cc',
        'run_all_unittests.cc',
        'skbitmap_operations_unittest.cc',
        'test_suite.cc',
        'test_suite.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
      ],
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            # TODO(brettw) re-enable this when the dependencies on WindowImpl are fixed!
            'canvas_direct2d_unittest.cc',
            'icon_util_unittest.cc',
            'native_theme_win_unittest.cc',
          ],
          'include_dirs': [
            '../..',
            '<(DEPTH)/third_party/wtl/include',
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
          }
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../../app/app.gyp:app_base',
            '../../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'gfx',
      'type': '<(library)',
      'msvs_guid': '13A8D36C-0467-4B4E-BAA3-FD69C45F076A',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../../third_party/libpng/libpng.gyp:libpng',
        '../../third_party/zlib/zlib.gyp:zlib',
        'gfx_resources',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'sources': [
        'blit.cc',
        'blit.h',
        'brush.h',
        'canvas.cc',
        'canvas.h',
        'canvas_skia.h',
        'canvas_skia.cc',
        'canvas_skia_linux.cc',
        'canvas_skia_mac.mm',
        'canvas_skia_paint.h',
        'canvas_skia_win.cc',
        'codec/jpeg_codec.cc',
        'codec/jpeg_codec.h',
        'codec/png_codec.cc',
        'codec/png_codec.h',
        'color_utils.cc',
        'color_utils.h',
        'compositor.cc',
        'compositor_gl.cc',
        'compositor.h',
        'favicon_size.h',
        'font.h',
        'font.cc',
        'gfx_paths.cc',
        'gfx_paths.h',
        'gfx_module.cc',
        'gfx_module.h',
        'image.cc',
        'image.h',
        'image_mac.mm',
        'insets.cc',
        'insets.h',
        'native_widget_types.h',
        'path.cc',
        'path.h',
        'path_gtk.cc',
        'path_win.cc',
        'platform_font.h',
        'platform_font_gtk.h',
        'platform_font_gtk.cc',
        'platform_font_mac.h',
        'platform_font_mac.mm',
        'platform_font_win.h',
        'platform_font_win.cc',
        'point.cc',
        'point.h',
        'rect.cc',
        'rect.h',
        'scoped_cg_context_state_mac.h',
        'scrollbar_size.cc',
        'scrollbar_size.h',
        'size.cc',
        'size.h',
        'skbitmap_operations.cc',
        'skbitmap_operations.h',
        'skia_util.cc',
        'skia_util.h',
        'skia_utils_gtk.cc',
        'skia_utils_gtk.h',
        'transform.h',
        'transform_skia.cc',
        'transform_skia.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'canvas_direct2d.cc',
            'canvas_direct2d.h',
            'gdi_util.cc',
            'gdi_util.h',
            'icon_util.cc',
            'icon_util.h',
            'native_theme_win.cc',
            'native_theme_win.h',
            'win_util.cc',
            'win_util.h',
          ],
          'include_dirs': [
            '../..',
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../../build/linux/system.gyp:fontconfig',
            '../../build/linux/system.gyp:gtk',
          ],
          'link_settings': {
            'libraries': [
              '-lGL',
            ],
          },
          'sources!': [
            'compositor.cc',
          ],
          'sources': [
            'gtk_native_view_id_manager.cc',
            'gtk_native_view_id_manager.h',
            'gtk_preserve_window.cc',
            'gtk_preserve_window.h',
            'gtk_util.cc',
            'gtk_util.h',
            'native_theme_linux.cc',
            'native_theme_linux.h',
            'native_widget_types_gtk.cc',
          ],
        }, {
          'sources!': [
            'compositor_gl.cc',
          ]
        }],
      ],
    },
    {
      # theme_resources also generates a .cc file, so it can't use the rules above.
      'target_name': 'gfx_resources',
      'type': 'none',
      'msvs_guid' : '5738AE53-E919-4987-A2EF-15FDBD8F90F6',
      'actions': [
        {
          'action_name': 'gfx_resources',
          'variables': {
            'input_path': 'gfx_resources.grd',
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': [
            '<@(grit_cmd)',
            '-i', '<(input_path)', 'build',
            '-o', '<(grit_out_dir)',
            '<@(grit_defines)',
          ],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../../build/win/system.gyp:cygwin'],
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

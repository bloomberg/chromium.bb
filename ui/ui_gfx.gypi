# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # TODO(rsesek): Remove this target once ui_unittests is run on the
      # waterfall instead of gfx_unittests.
      'target_name': 'gfx_unittests',
      'type': 'none',
      'msvs_guid': '1D386FA9-2501-41E2-8FE8-527DAF479CE6',
      'conditions': [
        ['inside_chromium_build==1', {
          'dependencies': [
            'ui_unittests',
          ],
        }],
      ],
      'actions': [
        {
          'message': 'TEMPORARY: Copying ui_unittests to gfx_unittests',
          'variables': {
            'ui_copy_target': '<(PRODUCT_DIR)/ui_unittests<(EXECUTABLE_SUFFIX)',
            'ui_copy_dest': '<(PRODUCT_DIR)/gfx_unittests<(EXECUTABLE_SUFFIX)',
          },
          'inputs': ['<(ui_copy_target)'],
          'outputs': ['<(ui_copy_dest)'],
          'action_name': 'TEMP_copy_ui_unittests',
          'action': [
            'python', '-c',
            'import os, shutil; ' \
            'shutil.copyfile(\'<(ui_copy_target)\', \'<(ui_copy_dest)\'); ' \
            'os.chmod(\'<(ui_copy_dest)\', 0700)'
          ]
        }
      ],
    },
    {
      'target_name': 'ui_gfx',
      'type': '<(library)',
      'msvs_guid': '13A8D36C-0467-4B4E-BAA3-FD69C45F076A',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        'gfx_resources',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'sources': [
        'gfx/blit.cc',
        'gfx/blit.h',
        'gfx/brush.h',
        'gfx/canvas.cc',
        'gfx/canvas.h',
        'gfx/canvas_skia.h',
        'gfx/canvas_skia.cc',
        'gfx/canvas_skia_linux.cc',
        'gfx/canvas_skia_mac.mm',
        'gfx/canvas_skia_paint.h',
        'gfx/canvas_skia_win.cc',
        'gfx/codec/jpeg_codec.cc',
        'gfx/codec/jpeg_codec.h',
        'gfx/codec/png_codec.cc',
        'gfx/codec/png_codec.h',
        'gfx/color_utils.cc',
        'gfx/color_utils.h',
        'gfx/favicon_size.h',
        'gfx/font.h',
        'gfx/font.cc',
        'gfx/gfx_paths.cc',
        'gfx/gfx_paths.h',
        'gfx/image.cc',
        'gfx/image.h',
        'gfx/image_mac.mm',
        'gfx/insets.cc',
        'gfx/insets.h',
        'gfx/native_theme.cc',
        'gfx/native_theme.h',
        'gfx/native_widget_types.h',
        'gfx/path.cc',
        'gfx/path.h',
        'gfx/path_gtk.cc',
        'gfx/path_win.cc',
        'gfx/platform_font.h',
        'gfx/platform_font_gtk.h',
        'gfx/platform_font_gtk.cc',
        'gfx/platform_font_mac.h',
        'gfx/platform_font_mac.mm',
        'gfx/platform_font_win.h',
        'gfx/platform_font_win.cc',
        'gfx/point.cc',
        'gfx/point.h',
        'gfx/rect.cc',
        'gfx/rect.h',
        'gfx/scoped_cg_context_save_gstate_mac.h',
        'gfx/scoped_ns_graphics_context_save_gstate_mac.h',
        'gfx/scoped_ns_graphics_context_save_gstate_mac.mm',
        'gfx/scrollbar_size.cc',
        'gfx/scrollbar_size.h',
        'gfx/size.cc',
        'gfx/size.h',
        'gfx/skbitmap_operations.cc',
        'gfx/skbitmap_operations.h',
        'gfx/skia_util.cc',
        'gfx/skia_util.h',
        'gfx/skia_utils_gtk.cc',
        'gfx/skia_utils_gtk.h',
        'gfx/transform.h',
        'gfx/transform.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'gfx/canvas_direct2d.cc',
            'gfx/canvas_direct2d.h',
            'gfx/gdi_util.cc',
            'gfx/gdi_util.h',
            'gfx/icon_util.cc',
            'gfx/icon_util.h',
            'gfx/native_theme_win.cc',
            'gfx/native_theme_win.h',
            'gfx/win_util.cc',
            'gfx/win_util.h',
          ],
          'include_dirs': [
            '../',
            '../third_party/wtl/include',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'gfx/gtk_native_view_id_manager.cc',
            'gfx/gtk_native_view_id_manager.h',
            'gfx/gtk_preserve_window.cc',
            'gfx/gtk_preserve_window.h',
            'gfx/gtk_util.cc',
            'gfx/gtk_util.h',
            'gfx/native_theme_linux.cc',
            'gfx/native_theme_linux.h',
            'gfx/native_widget_types_gtk.cc',
          ],
        }],
        ['chromeos==1', {
          'sources': [
            'gfx/native_theme_chromeos.cc',
            'gfx/native_theme_chromeos.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'gfx_resources',
      'type': 'none',
      'msvs_guid' : '5738AE53-E919-4987-A2EF-15FDBD8F90F6',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/gfx',
      },
      'actions': [
        {
          'action_name': 'gfx_resources',
          'variables': {
            'grit_grd_file': 'gfx/gfx_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },

  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

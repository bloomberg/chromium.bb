# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_test_support',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        'gfx/gfx.gyp:gfx',
        'gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        'base/test/ui_controls.h',
        'base/test/ui_controls_aura.cc',
        'base/test/ui_controls_internal_win.cc',
        'base/test/ui_controls_internal_win.h',
        'base/test/ui_controls_mac.mm',
        'base/test/ui_controls_win.cc',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['OS!="ios"', {
          'type': 'static_library',
          'includes': [ 'base/ime/ime_test_support.gypi' ],
        }, {  # OS=="ios"
          # None of the sources in this target are built on iOS, resulting in
          # link errors when building targets that depend on this target
          # because the static library isn't found. If this target is changed
          # to have sources that are built on iOS, the target should be changed
          # to be of type static_library on all platforms.
          'type': 'none',
          # The cocoa files don't apply to iOS.
          'sources/': [['exclude', 'cocoa']],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
            '../skia/skia.gyp:skia',
          ],
        }],
        ['use_aura==1', {
          'sources!': [
            'base/test/ui_controls_mac.mm',
            'base/test/ui_controls_win.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../url/url.gyp:url_lib',
        'base/strings/ui_strings.gyp:ui_strings',
        'base/ui_base.gyp:ui_base',
        'events/events.gyp:events_base',
        'gfx/gfx.gyp:gfx_test_support',
        'resources/ui_resources.gyp:ui_resources',
        'resources/ui_resources.gyp:ui_test_pak',
        'ui_test_support',
      ],
      # iOS uses a small subset of ui. common_sources are the only files that
      # are built on iOS.
      'common_sources' : [
        'base/layout_unittest.cc',
        'base/l10n/l10n_util_mac_unittest.mm',
        'base/l10n/l10n_util_unittest.cc',
        'base/l10n/l10n_util_win_unittest.cc',
        'base/l10n/time_format_unittest.cc',
        'base/models/tree_node_iterator_unittest.cc',
        'base/resource/data_pack_literal.cc',
        'base/resource/data_pack_unittest.cc',
        'base/resource/resource_bundle_unittest.cc',
        'base/test/run_all_unittests.cc',
        'gfx/animation/animation_container_unittest.cc',
        'gfx/animation/animation_unittest.cc',
        'gfx/animation/multi_animation_unittest.cc',
        'gfx/animation/slide_animation_unittest.cc',
        'gfx/codec/png_codec_unittest.cc',
        'gfx/color_utils_unittest.cc',
        'gfx/display_unittest.cc',
        'gfx/font_unittest.cc',
        'gfx/image/image_family_unittest.cc',
        'gfx/image/image_skia_unittest.cc',
        'gfx/image/image_unittest.cc',
        'gfx/image/image_unittest_util.cc',
        'gfx/image/image_unittest_util.h',
        'gfx/image/image_unittest_util_ios.mm',
        'gfx/image/image_unittest_util_mac.mm',
        'gfx/screen_unittest.cc',
        'gfx/shadow_value_unittest.cc',
        'gfx/skbitmap_operations_unittest.cc',
        'gfx/skrect_conversion_unittest.cc',
        'gfx/text_elider_unittest.cc',
        'gfx/text_utils_unittest.cc',
      ],
      'all_sources': [
        '<@(_common_sources)',
        'base/accelerators/accelerator_manager_unittest.cc',
        'base/accelerators/menu_label_accelerator_util_linux_unittest.cc',
        'base/clipboard/clipboard_unittest.cc',
        'base/clipboard/custom_data_helper_unittest.cc',
        'base/cocoa/base_view_unittest.mm',
        'base/cocoa/cocoa_base_utils_unittest.mm',
        'base/cocoa/controls/blue_label_button_unittest.mm',
        'base/cocoa/controls/hover_image_menu_button_unittest.mm',
        'base/cocoa/controls/hyperlink_button_cell_unittest.mm',
        'base/cocoa/focus_tracker_unittest.mm',
        'base/cocoa/fullscreen_window_manager_unittest.mm',
        'base/cocoa/hover_image_button_unittest.mm',
        'base/cocoa/menu_controller_unittest.mm',
        'base/cocoa/nsgraphics_context_additions_unittest.mm',
        'base/cocoa/tracking_area_unittest.mm',
        'base/dragdrop/os_exchange_data_provider_aurax11_unittest.cc',
        'base/gtk/gtk_expanded_container_unittest.cc',
        'base/models/list_model_unittest.cc',
        'base/models/list_selection_model_unittest.cc',
        'base/models/tree_node_model_unittest.cc',
        'base/test/data/resource.h',
        'base/text/bytes_formatting_unittest.cc',
        'base/view_prop_unittest.cc',
        'base/webui/web_ui_util_unittest.cc',
        'gfx/animation/tween_unittest.cc',
        'gfx/blit_unittest.cc',
        'gfx/break_list_unittest.cc',
        'gfx/canvas_unittest.cc',
        'gfx/canvas_unittest_mac.mm',
        'gfx/codec/jpeg_codec_unittest.cc',
        'gfx/color_analysis_unittest.cc',
        'gfx/font_list_unittest.cc',
        'gfx/image/image_mac_unittest.mm',
        'gfx/image/image_util_unittest.cc',
        'gfx/ozone/dri/hardware_display_controller_unittest.cc',
        'gfx/ozone/dri/dri_surface_factory_unittest.cc',
        'gfx/ozone/dri/dri_surface_unittest.cc',
        'gfx/platform_font_mac_unittest.mm',
        'gfx/render_text_unittest.cc',
        'gfx/sequential_id_generator_unittest.cc',
        'gfx/transform_util_unittest.cc',
        'gfx/utf16_indexing_unittest.cc',
      ],
      'includes': [
        'display/display_unittests.gypi',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['OS!="ios"', {
          'sources' : ['<@(_all_sources)'],
          'includes': [
            'base/ime/ime_unittests.gypi',
          ],
        }, {  # OS=="ios"
          'sources' : [
            '<@(_common_sources)',
          ],
          # The ResourceBundle unittest expects a locale.pak file to exist in
          # the bundle for English-US. Copy it in from where it was generated
          # by ui_resources.gyp:ui_test_pak.
          'mac_bundle_resources': [
            '<(PRODUCT_DIR)/ui/en.lproj/locale.pak',
          ],
        }],
        ['OS == "win"', {
          'sources': [
            'base/dragdrop/os_exchange_data_win_unittest.cc',
            'base/win/hwnd_subclass_unittest.cc',
            'gfx/font_fallback_win_unittest.cc',
            'gfx/icon_util_unittest.cc',
            'gfx/icon_util_unittests.rc',
            'gfx/platform_font_win_unittest.cc',
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
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['OS != "mac" and OS != "ios"', {
          'sources': [
            'gfx/transform_unittest.cc',
            'gfx/interpolated_transform_unittest.cc',
          ],
        }],
        ['OS == "android"', {
          'sources': [
            'gfx/android/scroller_unittest.cc',
          ],
        }],
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['use_pango == 1', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:pangocairo',
          ],
          'sources': [
            'gfx/platform_font_pango_unittest.cc',
          ],
          'conditions': [
            # TODO(dmikurube): Kill linux_use_tcmalloc. http://crbug.com/345554
            ['(use_allocator!="none" and use_allocator!="see_use_tcmalloc") or (use_allocator=="see_use_tcmalloc" and linux_use_tcmalloc==1)', {
               'dependencies': [
                 '../base/allocator/allocator.gyp:allocator',
               ],
            }],
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
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
        ['OS=="android" or OS=="ios"', {
          'sources!': [
            'gfx/render_text_unittest.cc',
          ],
        }],
        ['OS!="win" or use_aura==0', {
          'sources!': [
            'base/view_prop_unittest.cc',
          ],
        }],
        ['use_x11==1 and use_aura==1',  {
          'sources': [
            'base/cursor/cursor_loader_x11_unittest.cc',
          ],
        }],
        ['OS=="mac"',  {
          'dependencies': [
            'events/events.gyp:events_test_support',
            'gfx/gfx.gyp:gfx_test_support',
            'ui_unittests_bundle',
          ],
        }],
        ['use_aura==1 or toolkit_views==1',  {
          'sources': [
            'base/dragdrop/os_exchange_data_unittest.cc',
          ],
          'dependencies': [
            'events/events.gyp:events',
            'events/events.gyp:events_base',
            'events/events.gyp:events_test_support',
          ],
        }],
        ['use_aura==1', {
          'sources!': [
            'base/dragdrop/os_exchange_data_win_unittest.cc',
            'gfx/screen_unittest.cc',
          ],
        }],
        ['use_ozone==1', {
          'dependencies': [
          '<(DEPTH)/build/linux/system.gyp:dridrm',
          ],
        }],
        ['use_ozone==1 and use_pango==0', {
          'sources!': [
            'gfx/text_elider_unittest.cc',
            'gfx/font_unittest.cc',
            'gfx/font_list_unittest.cc',
            'gfx/render_text_unittest.cc',
            'gfx/canvas_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'base/dragdrop/os_exchange_data_provider_aurax11_unittest.cc',
          ],
        }],
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^base/l10n/l10n_util_mac_unittest\\.mm$'],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # Mac target to build a test Framework bundle to mock out resource loading.
    ['OS == "mac"', {
      'targets': [
        {
          'target_name': 'ui_unittests_bundle',
          'type': 'shared_library',
          'dependencies': [
            'resources/ui_resources.gyp:ui_test_pak',
          ],
          'includes': [ 'ui_unittests_bundle.gypi' ],
        },
      ],
    }],
    # Special target to wrap a gtest_target_type==shared_library
    # ui_unittests into an android apk for execution.
    # See base.gyp for TODO(jrg)s about this strategy.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'ui_unittests_apk',
          'type': 'none',
          'dependencies': [
            'ui_unittests',
          ],
          'variables': {
            'test_suite_name': 'ui_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)ui_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}

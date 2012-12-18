# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'webkit_compositor_bindings_sources': [
      'web_animation_curve_common.cc',
      'web_animation_curve_common.h',
      'web_animation_id_provider.cc',
      'web_animation_id_provider.h',
      'web_animation_impl.cc',
      'web_animation_impl.h',
      'web_compositor_support_output_surface.cc',
      'web_compositor_support_output_surface.h',
      'web_compositor_support_software_output_device.cc',
      'web_compositor_support_software_output_device.h',
      'web_content_layer_impl.cc',
      'web_content_layer_impl.h',
      'web_delegated_renderer_layer_impl.cc',
      'web_delegated_renderer_layer_impl.h',
      'web_external_texture_layer_impl.cc',
      'web_external_texture_layer_impl.h',
      'web_float_animation_curve_impl.cc',
      'web_float_animation_curve_impl.h',
      'web_io_surface_layer_impl.cc',
      'web_io_surface_layer_impl.h',
      'web_image_layer_impl.cc',
      'web_image_layer_impl.h',
      'web_layer_impl.cc',
      'web_layer_impl.h',
      'web_nine_patch_layer_impl.cc',
      'web_nine_patch_layer_impl.h',
      'web_to_ccinput_handler_adapter.cc',
      'web_to_ccinput_handler_adapter.h',
      'web_to_ccscrollbar_theme_painter_adapter.cc',
      'web_to_ccscrollbar_theme_painter_adapter.h',
      'web_layer_tree_view_impl.cc',
      'web_layer_tree_view_impl.h',
      'web_scrollbar_layer_impl.cc',
      'web_scrollbar_layer_impl.h',
      'web_solid_color_layer_impl.cc',
      'web_solid_color_layer_impl.h',
      'web_video_layer_impl.cc',
      'web_video_layer_impl.h',
      'web_transform_animation_curve_impl.cc',
      'web_transform_animation_curve_impl.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        'webkit_compositor_bindings',
      ],
      'sources': [
        'web_compositor_support_impl.cc',
        'web_compositor_support_impl.h',
      ],
      'includes': [
        '../../cc/cc.gypi',
      ],
      'include_dirs': [
        '../..',
        '../../cc',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(webkit_src_dir)/Source/Platform/chromium',
        '<@(cc_stubs_dirs)',
      ],
    },
    {
      'target_name': 'webkit_compositor_bindings',
      'type': 'static_library',
      'includes': [
        '../../cc/cc.gypi',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../skia/skia.gyp:skia',
        '../../ui/ui.gyp:ui',
      ],
      'include_dirs': [
        '../../cc',
        '<@(cc_stubs_dirs)',
        '<(webkit_src_dir)/Source/Platform/chromium',
      ],
      'sources': [
        '<@(webkit_compositor_bindings_sources)',
      ],
    },
  ],
}

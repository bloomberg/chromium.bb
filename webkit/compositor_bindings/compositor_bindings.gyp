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
      'web_compositor_support_software_output_device.cc',
      'web_compositor_support_software_output_device.h',
      'web_content_layer_impl.cc',
      'web_content_layer_impl.h',
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
      'web_to_ccvideo_frame_provider.cc',
      'web_to_ccvideo_frame_provider.h',
      'web_layer_tree_view_impl_for_testing.cc',
      'web_layer_tree_view_impl_for_testing.h',
      'web_scrollbar_layer_impl.cc',
      'web_scrollbar_layer_impl.h',
      'web_solid_color_layer_impl.cc',
      'web_solid_color_layer_impl.h',
      'web_transform_operations_impl.cc',
      'web_transform_operations_impl.h',
      'web_transformation_matrix_util.cc',
      'web_transformation_matrix_util.h',
      'web_video_layer_impl.cc',
      'web_video_layer_impl.h',
      'web_transform_animation_curve_impl.cc',
      'web_transform_animation_curve_impl.h',
    ],
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../../..',
      },{
        'webkit_src_dir': '../../third_party/WebKit',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../../cc/cc.gyp:cc',
        'webkit_compositor_bindings',
      ],
      'sources': [
        'web_compositor_support_impl.cc',
        'web_compositor_support_impl.h',
      ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
    },
    {
      'target_name': 'webkit_compositor_bindings',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../media/media.gyp:media',
        '../../skia/skia.gyp:skia',
        '../../ui/ui.gyp:ui',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        '<@(webkit_compositor_bindings_sources)',
      ],
      'defines': [
        'WEBKIT_COMPOSITOR_BINDINGS_IMPLEMENTATION=1'
      ]
    },
  ],
}

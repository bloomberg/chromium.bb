# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/cc/cc.gyp:cc',
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
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/gpu/gpu.gyp:gpu',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/webkit/common/gpu/webkit_gpu.gyp:webkit_gpu',
      ],
      'defines': [
        'WEBKIT_COMPOSITOR_BINDINGS_IMPLEMENTATION=1'
      ],
      'sources': [
        'scrollbar_impl.cc',
        'scrollbar_impl.h',
        'web_animation_curve_common.cc',
        'web_animation_curve_common.h',
        'web_animation_impl.cc',
        'web_animation_impl.h',
        'web_compositing_reasons.cc',
        'web_content_layer_impl.cc',
        'web_content_layer_impl.h',
        'web_external_bitmap_impl.cc',
        'web_external_bitmap_impl.h',
        'web_external_texture_layer_impl.cc',
        'web_external_texture_layer_impl.h',
        'web_filter_operations_impl.cc',
        'web_filter_operations_impl.h',
        'web_float_animation_curve_impl.cc',
        'web_float_animation_curve_impl.h',
        'web_image_layer_impl.cc',
        'web_image_layer_impl.h',
        'web_layer_impl.cc',
        'web_layer_impl.h',
        'web_layer_impl_fixed_bounds.cc',
        'web_layer_impl_fixed_bounds.h',
        'web_scrollbar_layer_impl.cc',
        'web_scrollbar_layer_impl.h',
        'web_solid_color_layer_impl.cc',
        'web_solid_color_layer_impl.h',
        'web_to_cc_animation_delegate_adapter.cc',
        'web_to_cc_animation_delegate_adapter.h',
        'web_transform_animation_curve_impl.cc',
        'web_transform_animation_curve_impl.h',
        'web_transform_operations_impl.cc',
        'web_transform_operations_impl.h',
      ],
    },
  ],
}

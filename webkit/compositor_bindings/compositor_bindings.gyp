# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'webkit_compositor_bindings_sources': [
      'CCThreadImpl.cpp',
      'CCThreadImpl.h',
      'WebAnimationCurveCommon.cpp',
      'WebAnimationCurveCommon.h',
      'WebAnimationImpl.cpp',
      'WebAnimationImpl.h',
      'WebCompositorImpl.cpp',
      'WebCompositorImpl.h',
      'WebContentLayerImpl.cpp',
      'WebContentLayerImpl.h',
      'WebExternalTextureLayerImpl.cpp',
      'WebExternalTextureLayerImpl.h',
      'WebFloatAnimationCurveImpl.cpp',
      'WebFloatAnimationCurveImpl.h',
      'WebIOSurfaceLayerImpl.cpp',
      'WebIOSurfaceLayerImpl.h',
      'WebImageLayerImpl.cpp',
      'WebImageLayerImpl.h',
      'WebLayerImpl.cpp',
      'WebLayerImpl.h',
      'WebToCCInputHandlerAdapter.cpp',
      'WebToCCInputHandlerAdapter.h',
      'WebLayerTreeViewImpl.cpp',
      'WebLayerTreeViewImpl.h',
      'WebScrollbarLayerImpl.cpp',
      'WebScrollbarLayerImpl.h',
      'WebSolidColorLayerImpl.cpp',
      'WebSolidColorLayerImpl.h',
      'WebVideoLayerImpl.cpp',
      'WebVideoLayerImpl.h',
      'WebTransformAnimationCurveImpl.cpp',
      'WebTransformAnimationCurveImpl.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
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
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(webkit_src_dir)/Source/Platform/chromium',
      ],
      'conditions': [
        ['use_libcc_for_compositor==1', {
          'include_dirs': [
            '../../cc',
            '<@(cc_stubs_dirs)',
          ],
          'dependencies': [
            'webkit_compositor_bindings',
            '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
          ],
          'defines': [
            'USE_LIBCC_FOR_COMPOSITOR',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_libcc_for_compositor==1', {
      'targets': [
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
            # We have to depend on WTF directly to pick up the correct defines for WTF headers - for instance USE_SYSTEM_MALLOC.
            '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
          ],
          'include_dirs': [
            '../../cc',
            '<@(cc_stubs_dirs)',
            '<(webkit_src_dir)/Source/Platform/chromium',
          ],
          'sources': [
            '<@(webkit_compositor_bindings_sources)',
            'webcore_convert.cc',
            'webcore_convert.h',
          ],
          'defines': [
            'USE_LIBCC_FOR_COMPOSITOR',
          ],
        },
      ],
    }],
  ],
}

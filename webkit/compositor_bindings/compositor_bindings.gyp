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
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        'web_compositor_support_impl.cc',
        'web_compositor_support_impl.h',
      ],
      'include_dirs': [
        '../..',
        '<(webkit_src_dir)/Source/Platform/chromium',
      ],
      'conditions': [
        ['use_libcc_for_compositor==1', {
          'include_dirs': [
            '../../cc',
            '../../cc/stubs',
          ],
          'dependencies': [
            'webkit_compositor_bindings',
            '../../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
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
          'dependencies': [
            '../../base/base.gyp:base',
            '../../cc/cc.gyp:cc',
            '../../skia/skia.gyp:skia',
            '../../third_party/WebKit/Source/Platform/Platform.gyp/Platform.gyp:webkit_platform',
            # We have to depend on WTF directly to pick up the correct defines for WTF headers - for instance USE_SYSTEM_MALLOC.
            '../../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
          ],
          'defines': [
            'WEBKIT_IMPLEMENTATION=1',
          ],
          'include_dirs': [
            '../../cc',
            '../../cc/stubs',
            'stubs',
            '../../third_party/WebKit/Source/WebKit/chromium/public',
          ],
          'sources': [
            '<@(webkit_compositor_bindings_sources)',
            'stubs/public/WebTransformationMatrix.h',
          ],
          'conditions': [
            ['component=="shared_library"', {
              'defines': [
                'WEBKIT_DLL',
              ],
            }],
          ]
        },
      ],
    }],
  ],
}

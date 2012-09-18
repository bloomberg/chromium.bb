# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'webkit_compositor_bindings_tests_sources': [
      'WebAnimationTest.cpp',
      'WebFloatAnimationCurveTest.cpp',
      'WebLayerTest.cpp',
      'WebLayerTreeViewTest.cpp',
      'WebTransformAnimationCurveTest.cpp',
      'WebTransformOperationsTest.cpp',
      'WebTransformationMatrixTest.cpp',
      'test/FakeWebScrollbarThemeGeometry.h',
      'test/WebLayerTreeViewTestCommon.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_bindings_unittests',
      'type' : 'executable',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['use_libcc_for_compositor==1', {
          'defines': [
            'USE_LIBCC_FOR_COMPOSITOR',
          ],
          'dependencies': [
            '../../cc/cc.gyp:cc',
            '../../cc/cc_tests.gyp:cc_test_support',
            '../../skia/skia.gyp:skia',
            '../../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            'compositor_bindings.gyp:webkit_compositor_bindings',
          ],
          'include_dirs': [
            '.',
            'test',
            '<(DEPTH)/cc',
            '<(DEPTH)/cc/stubs',
            '<(DEPTH)/cc/test',
            '../../third_party/WebKit/Source/Platform/chromium'
          ],
          'sources': [
            '<@(webkit_compositor_bindings_tests_sources)',
          ]
        }],
      ],
    },
  ],
}

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'webkit_compositor_bindings_tests_sources': [
      'web_animation_unittest.cc',
      'web_float_animation_curve_unittest.cc',
      'web_layer_unittest.cc',
      'web_layer_tree_view_unittest.cc',
      'web_transform_animation_curve_unittest.cc',
      'web_transform_operations_unittest.cc',
      'web_transformation_matrix_unittest.cc',
      'test/fake_web_scrollbar_theme_geometry.h',
      'test/web_layer_tree_view_test_common.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_bindings_unittests',
      'type' : 'executable',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../cc/cc.gyp:cc',
        '../../cc/cc_tests.gyp:cc_test_support',
        '../../skia/skia.gyp:skia',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
        'compositor_bindings.gyp:webkit_compositor_bindings',
      ],
      'sources': [
        '<@(webkit_compositor_bindings_tests_sources)',
        'test/run_all_unittests.cc',
      ],
      'include_dirs': [
        '.',
        'test',
        '<(DEPTH)/cc',
        '<(DEPTH)/cc/stubs',
        '<(DEPTH)/cc/test',
        '../../third_party/WebKit/Source/Platform/chromium'
      ],
    },
  ],
}

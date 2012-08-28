# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'webkit_compositor_tests_sources': [
      'WebFloatAnimationCurveTest.cpp',
      'WebTransformAnimationCurveTest.cpp',
    ],
  },
  'conditions': [
    ['use_libcc_for_compositor==1 and component!="shared_library"', {
      'targets': [
        {
          'target_name': 'webkit_compositor_unittests',
          'type' : 'executable',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/cc/cc.gyp:cc',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/WebKit/Source/Platform/Platform.gyp/Platform.gyp:webkit_platform',
            '<(DEPTH)/third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support',
            'compositor.gyp:webkit_compositor',
          ],
          'include_dirs': [
            '.',
            '<(DEPTH)/cc',
            '<(DEPTH)/cc/stubs',
          ],
          'sources': [
            '<@(webkit_compositor_tests_sources)',
            'test/run_all_unittests.cc',
          ]
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'webkit_compositor_unittests',
          'type' : 'none',
        }
      ],
    }],
  ],
}

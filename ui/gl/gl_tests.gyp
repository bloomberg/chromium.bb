# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gl_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'test/run_all_unittests.cc',
        'gpu_timing_unittest.cc',
        'gl_api_unittest.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/gl/gl.gyp:gl_unittest_utils',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS in ("win", "android", "linux")', {
          'sources': [
            'egl_api_unittest.cc',
            'test/egl_initialization_displays_unittest.cc',
          ],
        }],
      ],
    }
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'gl_unittests_apk',
          'type': 'none',
          'dependencies': [
            'gl_unittests',
          ],
          'variables': {
            'test_suite_name': 'gl_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'gl_unittests_run',
          'type': 'none',
          'dependencies': [
            'gl_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'gl_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

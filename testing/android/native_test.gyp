# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # GN: //testing/android:native_test_native_code
          'target_name': 'native_test_native_code',
          'message': 'building native pieces of native test package',
          'type': 'static_library',
          'sources': [
            'native_test/native_test_jni_onload.cc',
            'native_test/native_test_launcher.cc',
            'native_test/native_test_launcher.h',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:test_support_base',
            '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../gtest.gyp:gtest',
            'native_test_jni_headers',
            'native_test_util',
          ],
        },
        {
          # GN: //testing/android:native_test_jni_headers
          'target_name': 'native_test_jni_headers',
          'type': 'none',
          'sources': [
            'native_test/java/src/org/chromium/native_test/NativeTestActivity.java'
          ],
          'variables': {
            'jni_gen_package': 'testing',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          # GN: //testing/android:native_test_util
          'target_name': 'native_test_util',
          'type': 'static_library',
          'sources': [
            'native_test/native_test_util.cc',
            'native_test/native_test_util.h',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
          ],
        },
      ],
    }]
  ],
}

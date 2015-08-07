# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //sync/android:sync_java
          'target_name': 'sync_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../sync/android/java',
          },
          'dependencies': [
            'model_type_java',
            'sync_jni_headers',
            '../base/base.gyp:base_java',
            '../net/net.gyp:net_java',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
            '../third_party/jsr-305/jsr-305.gyp:jsr_305_javalib',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'sync_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/sync/ModelTypeHelper.java',
          ],
          'variables': {
            'jni_gen_package': 'sync',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //chrome/android:chrome_android_java_enums_srcjar
          'target_name': 'model_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'internal_api/public/base/model_type.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}

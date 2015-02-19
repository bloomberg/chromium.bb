# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //third_party/robolectric:android_all_java
      'target_name': 'android_all_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/android-all-4.3_r2-robolectric-0.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/robolectric:tagsoup_java
      'target_name': 'tagsoup_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/tagsoup-1.2.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/robolectric:json_java
      'target_name': 'json_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/json-20080701.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/robolectric:robolectric_java
      'target_name': 'robolectric_jar',
      'type': 'none',
      'dependencies': [
        'android_all_jar',
        'tagsoup_jar',
        'json_jar',
      ],
      'variables': {
        'jar_path': 'lib/robolectric-2.4-jar-with-dependencies.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
  ],
}


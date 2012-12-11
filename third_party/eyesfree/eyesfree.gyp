# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'eyesfree_java',
      'type': 'none',
      'dependencies': [
        'eyesfree_aidl',
      ],
      'variables': {
        'package_name': 'eyesfree',
        'java_in_dir': 'src/android/java',
      },
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': 'eyesfree_aidl',
      'type': 'none',
      'variables': {
        'package_name': 'eyesfree',
        'aidl_interface_file': '<(android_sdk)/framework.aidl',
        'aidl_import_include': 'android/java/src',
      },
      'sources': [
        'src/android/java/src/com/googlecode/eyesfree/braille/selfbraille/ISelfBrailleService.aidl',
        'src/android/java/src/com/googlecode/eyesfree/braille/selfbraille/WriteData.aidl',
      ],
      'includes': [ '../../build/java_aidl.gypi' ],
    },
  ],
}

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'window_open_disposition_java',
      'type': 'none',
      'sources': [
        'java/WindowOpenDisposition.template',
      ],
      'variables': {
        'package_name': 'org/chromium/ui',
        'template_deps': ['../base/window_open_disposition_list.h'],
      },
      'includes': [ '../../build/android/java_cpp_template.gypi' ],
    },
    {
      'target_name': 'ui_java',
      'type': 'none',
      'variables': {
        'java_in_dir': '../../ui/android/java',
        'has_java_resources': 1,
        'R_package': 'org.chromium.ui',
        'R_package_relpath': 'org/chromium/ui',
        'java_strings_grd': 'android_ui_strings.grd',
      },
      'dependencies': [
        '../../base/base.gyp:base_java',
        'window_open_disposition_java',
      ],
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}

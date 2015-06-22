# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //third_party/errorprone:errorprone_java
      'target_name': 'errorprone_java',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/error_prone_core-1.1.2.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/errorprone:chromium_errorprone
      'target_name': 'chromium_errorprone',
      'type': 'none',
      'variables': {
        'src_paths': [
          'src/org/chromium/errorprone/ChromiumErrorProneCompiler.java',
        ],
        'enable_errorprone': 0,
      },
      'dependencies': [
        '../../build/android/setup.gyp:sun_tools_java',
        'errorprone_java',
      ],
      'includes': [
        '../../build/host_jar.gypi',
      ],
      'actions': [
        {
          'action_name': 'create_errorprone_binary_script',
          'inputs': [
            '<(DEPTH)/build/android/gyp/create_java_binary_script.py',
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            # Ensure that the script is touched when the jar is.
            '<(jar_path)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/bin.java/chromium_errorprone'
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/create_java_binary_script.py',
            '--output', '<(PRODUCT_DIR)/bin.java/chromium_errorprone',
            '--classpath=>@(input_jars_paths)',
            '--jar-path=<(jar_path)',
            '--main-class=org.chromium.errorprone.ChromiumErrorProneCompiler',
          ],
        },
      ],
    },
  ],
}

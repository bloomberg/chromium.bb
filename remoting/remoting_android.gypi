# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'remoting_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/chromoting/jni/JniInterface.java',
          ],
          'variables': {
            'jni_gen_package': 'remoting',
            'jni_generator_ptr_type': 'long',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },  # end of target 'remoting_jni_headers'
        {
          'target_name': 'remoting_client_jni',
          'type': 'shared_library',
          'dependencies': [
            'remoting_base',
            'remoting_client',
            'remoting_jni_headers',
            'remoting_protocol',
            '../google_apis/google_apis.gyp:google_apis',
          ],
          'sources': [
            'client/jni/android_keymap.cc',
            'client/jni/android_keymap.h',
            'client/jni/chromoting_jni_instance.cc',
            'client/jni/chromoting_jni_instance.h',
            'client/jni/chromoting_jni_onload.cc',
            'client/jni/chromoting_jni_runtime.cc',
            'client/jni/chromoting_jni_runtime.h',
            'client/jni/jni_frame_consumer.cc',
            'client/jni/jni_frame_consumer.h',
          ],
        },  # end of target 'remoting_client_jni'
        {
          'target_name': 'remoting_android_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/drawable',
              'files': [
                'resources/chromoting128.png',
                'resources/icon_host.png',
              ],
            },
          ],
        },  # end of target 'remoting_android_resources'
        {
          'target_name': 'remoting_apk_manifest',
          'type': 'none',
          'sources': [
            'android/java/AndroidManifest.xml.jinja2',
          ],
          'rules': [{
            'rule_name': 'generate_manifest',
            'extension': 'jinja2',
            'inputs': [
              '<(remoting_localize_path)',
              '<(branding_path)',
              '<(RULE_INPUT_PATH)',
            ],
            'outputs': [
              '<(SHARED_INTERMEDIATE_DIR)/remoting/android/<(RULE_INPUT_ROOT)',
            ],
            'action': [
              'python', '<(remoting_localize_path)',
              '--variables', '<(branding_path)',
              '--template', '<(RULE_INPUT_PATH)',
              '--locale_output', '<@(_outputs)',
              'en',
            ],
          }],
        },  # end of target 'remoting_apk_manifest'
        {
          'target_name': 'remoting_apk',
          'type': 'none',
          'dependencies': [
            'remoting_android_resources',
            'remoting_apk_manifest',
            'remoting_client_jni',
          ],
          'variables': {
            'apk_name': '<!(python <(version_py_path) -f <(branding_path) -t "@APK_FILE_NAME@")',
            'android_app_version_name': '<(version_full)',
            'android_app_version_code': '<!(python tools/android_version.py <(android_app_version_name))',
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/AndroidManifest.xml',
            'native_lib_target': 'libremoting_client_jni',
            'java_in_dir': 'android/java',
            'R_package': 'org.chromium.chromoting',
            'package_name': '<(_target_name)',
            'resource_dir': 'resources/android',
            'additional_res_dirs': [ '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res' ],
            'additional_input_paths': [
              '<(PRODUCT_DIR)/obj/remoting/remoting_android_resources.actions_rules_copies.stamp',
              '<(PRODUCT_DIR)/obj/remoting/remoting_resources.actions_rules_copies.stamp',
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # end of target 'remoting_apk'
        {
          # remoting_apk creates a .jar file as a side effect. Any Java targets
          # that need that .jar in their classpath should depend on this target.
          'target_name': 'remoting_apk_java',
          'type': 'none',
          'dependencies': [
            'remoting_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },  # end of target 'remoting_apk_java'
        {
          'target_name': 'remoting_test_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java_test_support',
            'remoting_apk_java',
          ],
          'variables': {
            'apk_name': 'ChromotingTest',
            'java_in_dir': 'android/javatests',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # end of target 'remoting_test_apk'
      ],  # end of 'targets'
    }],  # 'OS=="android"'

    ['OS=="android" and gtest_target_type=="shared_library"', {
      'targets': [
        {
          'target_name': 'remoting_unittests_apk',
          'type': 'none',
          'dependencies': [
            'remoting_unittests',
          ],
          'variables': {
            'test_suite_name': 'remoting_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)remoting_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],  # 'OS=="android" and gtest_target_type=="shared_library"'
  ],  # end of 'conditions'
}

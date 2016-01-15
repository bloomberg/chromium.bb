# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'variables': {
        # These hooks allow official builds to modify the remoting_apk target:
        # Official build of remoting_apk pulls in extra code.
        'remoting_apk_extra_dependencies%': [],
        # A different ProGuard config for Google Play Services is needed since the one used by
        # Chromium and Google Chrome strips out code that we need.
        'remoting_android_google_play_services_javalib%': '../third_party/android_tools/android_tools.gyp:google_play_services_javalib',
        # Allows official builds to define the ApplicationContext class differently, and provide
        # different implementations of parts of the product.
        'remoting_apk_java_in_dir%': 'android/apk',
      },
      'targets': [
        {
          'target_name': 'remoting_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/chromoting/jni/Client.java',
            'android/java/src/org/chromium/chromoting/jni/JniInterface.java',
            'android/java/src/org/chromium/chromoting/jni/TouchEventData.java',
          ],
          'variables': {
            'jni_gen_package': 'remoting',
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
            '../ui/events/events.gyp:dom_keycode_converter',
            '../ui/gfx/gfx.gyp:gfx',
          ],
          'sources': [
            'client/jni/android_keymap.cc',
            'client/jni/android_keymap.h',
            'client/jni/chromoting_jni_instance.cc',
            'client/jni/chromoting_jni_instance.h',
            'client/jni/chromoting_jni_runtime.cc',
            'client/jni/chromoting_jni_runtime.h',
            'client/jni/jni_client.cc',
            'client/jni/jni_client.h',
            'client/jni/jni_frame_consumer.cc',
            'client/jni/jni_frame_consumer.h',
            'client/jni/jni_touch_event_data.cc',
            'client/jni/jni_touch_event_data.h',
            'client/jni/remoting_jni_onload.cc',
            'client/jni/remoting_jni_registrar.cc',
            'client/jni/remoting_jni_registrar.h',
          ],
        },  # end of target 'remoting_client_jni'
        {
          'target_name': 'remoting_android_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/raw',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
                'webapp/base/html/credits_css.css',
                'webapp/base/html/main.css',
                'webapp/base/js/credits_js.js',
              ],
            },
          ],
          'dependencies': [
            'remoting_client_credits',
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
            ],
            'outputs': [
              '<(SHARED_INTERMEDIATE_DIR)/remoting/android/<(RULE_INPUT_ROOT)',
            ],
            'action': [
              'python', '<(remoting_localize_path)',
              '--variables', '<(branding_path)',
              '--template', '<(RULE_INPUT_PATH)',
              '--locale_output', '<@(_outputs)',
              '--define', 'ENABLE_CARDBOARD=<(enable_cardboard)',
              'en',
            ],
          }],
        },  # end of target 'remoting_apk_manifest'
        {
          'target_name': 'remoting_android_client_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.chromoting',
            'R_package_relpath': 'org/chromium/chromoting',
            'res_extra_dirs': [ '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res' ],
            'res_extra_files': [
              '<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(SHARED_INTERMEDIATE_DIR)" resources/remoting_strings.grd)',
            ],
          },
          'dependencies': [
            'remoting_android_resources',
            '../base/base.gyp:base_java',
            '../ui/android/ui_android.gyp:ui_java',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_appcompat_javalib',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_mediarouter_javalib',
            '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
            '../third_party/cardboard-java/cardboard.gyp:cardboard_jar',
            '<(remoting_android_google_play_services_javalib)',
          ],
          'includes': [ '../build/java.gypi' ],
          'conditions' : [
            ['enable_cast==1', {
              'variables': {
                'additional_src_dirs': [
                  'android/cast',
                ],
              },
            }],
          ],
        },  # end of target 'remoting_android_client_java'
        {
          # TODO(lambroslambrou): Move some of this to third_party/cardboard-java/ in case it is
          # useful to other clients. Also implement this for GN builds.
          'target_name': 'remoting_cardboard_extract_native_lib',
          'type': 'none',
          'actions': [
            {
              'action_name': 'extract_cardboard_native_lib',
              'inputs': [
                '../third_party/cardboard-java/src/CardboardSample/libs/cardboard.jar',
              ],
              'outputs': [
                '<(SHARED_LIB_DIR)/libvrtoolkit.so',
              ],
              'action': [
                'python',
                'tools/extract_android_native_lib.py',
                '<(android_app_abi)',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            },
          ],
        },  # end of target 'remoting_cardboard_extract_native_lib'
        {
          'target_name': 'remoting_apk',
          'type': 'none',
          'dependencies': [
            'remoting_apk_manifest',
            'remoting_client_jni',
            'remoting_android_client_java',
            '<@(remoting_apk_extra_dependencies)',
          ],
          'variables': {
            'apk_name': 'Chromoting',
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/AndroidManifest.xml',
            'java_in_dir': '<(remoting_apk_java_in_dir)',
            'native_lib_target': 'libremoting_client_jni',
          },
          'includes': [ '../build/java_apk.gypi' ],
          'conditions': [
            ['target_arch == "arm"', {
              'dependencies': [ 'remoting_cardboard_extract_native_lib' ],
              'variables': {
                'extra_native_libs': [ '<(SHARED_LIB_DIR)/libvrtoolkit.so' ],
              },
            }],
          ],
        },  # end of target 'remoting_apk'
        {
          'target_name': 'remoting_test_apk_manifest',
          'type': 'none',
          'sources': [
            'android/javatests/AndroidManifest.xml.jinja2',
          ],
          'rules': [{
            'rule_name': 'generate_manifest',
            'extension': 'jinja2',
            'inputs': [
              '<(remoting_localize_path)',
              '<(branding_path)',
            ],
            'outputs': [
              '<(SHARED_INTERMEDIATE_DIR)/remoting/android_test/<(RULE_INPUT_ROOT)',
            ],
            'action': [
              'python', '<(remoting_localize_path)',
              '--variables', '<(branding_path)',
              '--template', '<(RULE_INPUT_PATH)',
              '--locale_output', '<@(_outputs)',
              'en',
            ],
          }],
        },  # end of target 'remoting_test_apk_manifest'
        {
          'target_name': 'remoting_test_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java_test_support',
            'remoting_android_client_java',
            'remoting_test_apk_manifest',
          ],
          'variables': {
            'apk_name': 'ChromotingTest',
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/remoting/android_test/AndroidManifest.xml',
            'java_in_dir': 'android/javatests',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # end of target 'remoting_test_apk'
        {
          'target_name': 'remoting_unittests_apk',
          'type': 'none',
          'dependencies': [
            'remoting_unittests',
          ],
          'variables': {
            'test_suite_name': 'remoting_unittests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },  # end of target 'remoting_unittests_apk'
      ],  # end of 'targets'
    }],  # 'OS=="android"
  ],  # end of 'conditions'
}

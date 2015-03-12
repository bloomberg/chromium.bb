# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_enable.gypi',
  ],

  'targets': [
    {
      'target_name': 'remoting_all',
      'type': 'none',
      'dependencies': [
        '../remoting/remoting.gyp:remoting_base',
        '../remoting/remoting.gyp:remoting_breakpad',
        '../remoting/remoting.gyp:remoting_browser_test_resources',
        '../remoting/remoting.gyp:remoting_client',
        '../remoting/remoting.gyp:remoting_client_plugin',
        '../remoting/remoting.gyp:remoting_protocol',
        '../remoting/remoting.gyp:remoting_test_common',
        '../remoting/remoting.gyp:remoting_unittests',
        '../remoting/remoting.gyp:remoting_resources',
        '../remoting/remoting.gyp:remoting_webapp',
        '../remoting/remoting.gyp:remoting_webapp_html',
        '../remoting/remoting.gyp:remoting_webapp_unittests',
        '../remoting/app_remoting_webapp.gyp:ar_sample_app',
      ],

      'conditions' : [
        ['OS=="win"', {
          'dependencies': [
            '../remoting/remoting.gyp:remoting_breakpad_tester',
            '../remoting/remoting.gyp:remoting_console',
            '../remoting/remoting.gyp:remoting_core',
            '../remoting/remoting.gyp:remoting_desktop',
            '../remoting/remoting.gyp:remoting_host_installation',
            '../remoting/remoting.gyp:remoting_windows_resources',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../remoting/remoting.gyp:remoting_android_client_java',
            '../remoting/remoting.gyp:remoting_android_resources',
            '../remoting/remoting.gyp:remoting_apk',
            '../remoting/remoting.gyp:remoting_apk_manifest',
            '../remoting/remoting.gyp:remoting_client_jni',
            '../remoting/remoting.gyp:remoting_jni_headers',
            '../remoting/remoting.gyp:remoting_test_apk',
            '../remoting/remoting.gyp:remoting_unittests_apk',
          ],
        }],
        ['enable_remoting_host==1', {
          'dependencies': [
            '../remoting/remoting.gyp:remoting_infoplist_strings',
            '../remoting/remoting.gyp:remoting_it2me_host_static',
            '../remoting/remoting.gyp:remoting_it2me_native_messaging_host',
            '../remoting/remoting.gyp:remoting_host',
            '../remoting/remoting.gyp:remoting_host_setup_base',
            '../remoting/remoting.gyp:remoting_native_messaging_base',
            '../remoting/remoting.gyp:remoting_native_messaging_manifests',
            '../remoting/remoting.gyp:remoting_perftests',
            '../remoting/remoting.gyp:remoting_start_host',
          ],
        }],
        ['enable_me2me_host==1', {
          'dependencies': [
            '../remoting/remoting.gyp:remoting_me2me_host',
            '../remoting/remoting.gyp:remoting_me2me_host_archive',
            '../remoting/remoting.gyp:remoting_me2me_host_static',
            '../remoting/remoting.gyp:remoting_me2me_native_messaging_host',
          ],
        }],
      ],

    },  # end of target 'remoting_all'
  ],  # end of targets
}

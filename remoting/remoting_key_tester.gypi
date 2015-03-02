# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common_untrusted.gypi',
  ],

  'variables': {
    'remoting_key_tester_js_files': [
        'tools/javascript_key_tester/background.js',
        'tools/javascript_key_tester/chord_tracker.js',
        'tools/javascript_key_tester/event_listeners.js',
        'tools/javascript_key_tester/main.js',
     ],
  },

  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'remoting_key_tester',
          'type': 'none',
          'dependencies': [
            'remoting_key_tester_pexe',
            'remoting_key_tester_jscompile',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/remoting/key_tester',
              'files': [
                '<@(remoting_key_tester_js_files)',
                'tools/javascript_key_tester/main.css',
                'tools/javascript_key_tester/main.html',
                'tools/javascript_key_tester/manifest.json',
                'tools/javascript_key_tester/icon_128.png',
                'tools/javascript_key_tester/pnacl/remoting_key_tester.nmf',
                '<(PRODUCT_DIR)/remoting_key_tester_newlib.pexe',
              ],
            }
          ],
        },  # end of target 'remoting_key_tester'

        {
          'target_name': 'remoting_key_tester_jscompile',
          'type': 'none',
          'conditions': [
            ['run_jscompile != 0', {
              'variables': {
                'success_stamp': '<(PRODUCT_DIR)/<(_target_name).stamp',
              },
              'actions': [
                {
                  'action_name': 'jscompile remoting_key_tester',
                  'inputs': [
                    '<@(remoting_key_tester_js_files)',
                    'webapp/js_proto/chrome_proto.js',
                    'webapp/js_proto/chrome_event_proto.js',
                  ],
                  'outputs': [
                    '<(success_stamp)',
                  ],
                  'action': [
                    'python', '../third_party/closure_compiler/checker.py',
                    '--strict',
                    '--no-single-file',
                    '--success-stamp', '<(success_stamp)',
                    '<@(remoting_key_tester_js_files)',
                    'webapp/js_proto/chrome_proto.js',
                    'webapp/js_proto/chrome_event_proto.js',
                  ],
                },
              ],  # actions
            }],
          ],
        },  # end of target 'remoting_key_tester_jscompile'

        {
          'target_name': 'remoting_key_tester_pexe',
          'type': 'none',
          'sources': [
            'tools/javascript_key_tester/pnacl/remoting_key_tester.cc',
          ],
          'variables': {
            'nexe_target': 'remoting_key_tester',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_pnacl_newlib': 1,
            'extra_deps_pnacl_newlib': [
              '>(tc_lib_dir_pnacl_newlib)/libppapi.a',
              '>(tc_lib_dir_pnacl_newlib)/libppapi_cpp.a',
            ],
          },
          'link_flags': [
            '-lppapi_stub',
            '-lppapi_cpp',
          ],
          'dependencies': [
            # TODO(mseaborn): Remove need for this (https://crbug.com/456902).
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
        },  # end of target 'remoting_key_tester_pexe'
      ],
    }]
  ],
}

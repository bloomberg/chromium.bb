# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../remoting/remoting_locales.gypi',
    '../remoting/app_remoting_webapp_build.gypi',
  ],

  'target_defaults': {
    'variables': {
      'ar_internal': 0,
    },

    'conditions': [
      ['run_jscompile != 0', {
        'actions': [
          {
            'action_name': 'Verify >(ar_app_name) main.html',
            'variables': {
              'success_stamp': '<(PRODUCT_DIR)/>(_target_name)_main_jscompile.stamp',
            },
            'inputs': [
              '<@(ar_main_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              # Include zip as input so that this action is run after the build.
              '<(zip_path)',
            ],
            'outputs': [
              '<(success_stamp)',
            ],
            'action': [
              'python', '../third_party/closure_compiler/checker.py',
              '--strict',
              '--no-single-file',
              '--success-stamp', '<(success_stamp)',
              '<@(ar_main_js_files)',
              '<@(remoting_webapp_js_proto_files)',
            ],
          },
          {
            'action_name': 'Verify >(ar_app_name) background.js',
            'variables': {
              'success_stamp': '<(PRODUCT_DIR)/>(_target_name)_background_jscompile.stamp',
            },
            'inputs': [
              '<@(ar_background_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              # Include zip as input so that this action is run after the build.
              '<(zip_path)',
            ],
            'outputs': [
              '<(success_stamp)',
            ],
            'action': [
              'python', '../third_party/closure_compiler/checker.py',
              '--strict',
              '--no-single-file',
              '--success-stamp', '<(success_stamp)',
              '<@(ar_background_js_files)',
              '<@(remoting_webapp_js_proto_files)',
            ],
          },
          {
            'action_name': 'Verify >(ar_app_name) feedback_consent.html',
            'variables': {
              'success_stamp': '<(PRODUCT_DIR)/>(_target_name)_feedback_consent_jscompile.stamp',
            },
            'inputs': [
              '<@(ar_feedback_consent_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              # Include zip as input so that this action is run after the build.
              '<(zip_path)',
            ],
            'outputs': [
              '<(success_stamp)',
            ],
            'action': [
              'python', '../third_party/closure_compiler/checker.py',
              '--strict',
              '--no-single-file',
              '--success-stamp', '<(success_stamp)',
              '<@(ar_feedback_consent_js_files)',
              '<@(remoting_webapp_js_proto_files)',
            ],
          },
        ],  # actions
      }],
    ],  # conditions
  },

  'targets': [
    {
      # Sample AppRemoting app.
      'target_name': 'ar_sample_app',
      'app_key': 'Sample_App',
      'app_id': 'ljacajndfccfgnfohlgkdphmbnpkjflk',
      'app_client_id': 'sample_client_id',
      'app_name': 'App Remoting Client',
      'app_description': 'App Remoting client',
      'app_capabilities': ['GOOGLE_DRIVE'],
    },
  ],  # end of targets
}

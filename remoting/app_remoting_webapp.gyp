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
        'dependencies': [
          'app_remoting_webapp_compile.gypi:verify_main.html',
          'app_remoting_webapp_compile.gypi:verify_background.js',
          'app_remoting_webapp_compile.gypi:verify_feedback_consent.html',
        ],
      'inputs': [
        # Include zip as input so that this action is run after the build.
        '<(zip_path)',
      ],
      }],
    ], # conditions
  },
  'targets': [
    {
      # GN version: //remoting/webapp:ar_sample_app
      # Sample AppRemoting app.
      'target_name': 'ar_sample_app',
      'app_key': 'Sample_App',
      'app_id': 'ljacajndfccfgnfohlgkdphmbnpkjflk',
      'app_client_id': 'sample_client_id',
      'app_name': 'App Remoting Client',
      'app_description': 'App Remoting client',
      'app_capabilities': ['GOOGLE_DRIVE', 'CLOUD_PRINT'],
      'manifest_key': 'remotingdevbuild',
    },
  ],  # end of targets
}

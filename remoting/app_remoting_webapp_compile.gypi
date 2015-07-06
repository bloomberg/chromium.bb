# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
      '../remoting/remoting_webapp_files.gypi',
      '../remoting/app_remoting_webapp_files.gypi',
   ],
  'targets': [
    {
      'target_name': 'verify_main.html',
      'variables': {
        'source_files': [
          '<@(ar_main_js_files)',
          '<@(remoting_webapp_js_proto_files)',
        ],
        'out_file': '<(PRODUCT_DIR)/>(_target_name)_main_jscompile.stamp',
      },
       'includes': ['compile_js.gypi'],
    },
    {
      'target_name': 'verify_background.js',
      'variables': {
        'source_files': [
          '<@(ar_background_html_js_files)',
          '<@(remoting_webapp_js_proto_files)',
        ],
        'out_file': '<(PRODUCT_DIR)/>(_target_name)_background_jscompile.stamp',
      },
       'includes': ['compile_js.gypi'],
    },
    {
      'target_name': 'verify_feedback_consent.html',
      'variables': {
        'source_files': [
          '<@(ar_feedback_consent_html_all_js_files)',
          '<@(remoting_webapp_js_proto_files)',
        ],
        'out_file': '<(PRODUCT_DIR)/>(_target_name)_feedback_consent_jscompile.stamp',
      },
       'includes': ['compile_js.gypi'],
    },
  ],  # targets
}

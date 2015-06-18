# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# File in charge of Closure compiling remoting's webapp.

{
  'variables': {
    'success_stamp': '<(PRODUCT_DIR)/<(_target_name)_jscompile.stamp',
    'success_stamp_bt': '<(PRODUCT_DIR)/<(_target_name)_bt_jscompile.stamp',
    'success_stamp_ut': '<(PRODUCT_DIR)/<(_target_name)_ut_jscompile.stamp',
    'externs': [
      '<(DEPTH)/third_party/closure_compiler/externs/chrome_extensions.js',
      '<@(remoting_webapp_js_externs)',
    ],
    'compiler_flags': [
      '--strict',
      '--no-single-file',
      '--externs',
      '<(externs)',
    ],
  },
  'actions': [
    {
      'action_name': 'Verify remoting webapp',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_js_proto_files)',
      ],
      'outputs': [
        '<(success_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/third_party/closure_compiler/compile.py',
        '<@(compiler_flags)',
        '--success-stamp', '<(success_stamp)',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_js_proto_files)',
      ],
    },
    {
      'action_name': 'Verify remoting webapp with browsertests',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_browsertest_all_js_files)',
        '<@(remoting_webapp_browsertest_js_proto_files)',
      ],
      'outputs': [
        '<(success_stamp_bt)',
      ],
      'action': [
        'python', '<(DEPTH)/third_party/closure_compiler/compile.py',
        '<@(compiler_flags)',
        '--success-stamp', '<(success_stamp_bt)',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_browsertest_all_js_files)',
        '<@(remoting_webapp_browsertest_js_proto_files)',
      ],
    },
    {
      'action_name': 'Verify remoting webapp unittests',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_unittests_all_js_files)',
        '<@(remoting_webapp_unittests_js_proto_files)',
      ],
      'outputs': [
        '<(success_stamp_ut)',
      ],
      'action': [
        'python', '<(DEPTH)/third_party/closure_compiler/compile.py',
        '<@(compiler_flags)',
        '--success-stamp', '<(success_stamp_ut)',
        '<@(remoting_webapp_crd_js_files)',
        '<@(remoting_webapp_unittests_all_js_files)',
        '<@(remoting_webapp_unittests_js_proto_files)',
      ],
    },
  ],
  'includes': ['remoting_webapp_files.gypi'],
}

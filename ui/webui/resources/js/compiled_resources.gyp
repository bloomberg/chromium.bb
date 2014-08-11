# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'assert',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'event_tracker',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'load_time_data',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'util',
      'variables': {
        'depends': ['cr.js'],
        'externs': ['<(CLOSURE_DIR)/externs/chrome_send_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'local_strings',
      'variables': {
        'externs': ['template_data_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'parse_html_subset',
      'variables': {
        'externs': ['<(CLOSURE_DIR)/externs/pending_compiler_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'i18n_template_no_process',
      'variables': {
        'depends': ['load_time_data.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'i18n_template',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'i18n_template2',
      'variables': {
        'depends': ['load_time_data.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}

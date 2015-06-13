# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'ui_account_tweaks',
      'variables': {
        'depends': [
          '../compiled_resources.gyp:cr',
          '../compiled_resources.gyp:load_time_data',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ]
}

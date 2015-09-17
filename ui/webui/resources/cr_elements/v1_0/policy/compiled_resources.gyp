# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_policy_indicator',
      'variables': {
        'depends': [
          '../../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
        ],
        'externs': [
          '../../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_shared_menu',
      'variables': {
        'depends': [
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-list/iron-list-extracted.js',
          '../../js/assert.js',
          '../../js/cr.js',
          '../../js/cr/ui/position_util.js',
          '../../js/util.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}

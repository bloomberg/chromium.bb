# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_network_icon',
      'variables': {
        'depends': [
          'cr_onc_types.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_network_list',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          'cr_onc_types.js',
          'cr_network_list_item.js',
          '../../../../../ui/webui/resources/js/load_time_data.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_network_select',
      'variables': {
        'depends': [
          'compiled_resources.gyp:cr_network_list',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    }, 
  ],
}

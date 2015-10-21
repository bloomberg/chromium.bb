# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_policy_indicator_behavior',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_policy_pref_behavior',
      'variables': {
        'depends': [
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-iconset-svg/iron-iconset-svg-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-meta/iron-meta-extracted.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          'cr_policy_indicator_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_policy_pref_indicator',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          'cr_policy_indicator_behavior.js',
          'cr_policy_pref_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_policy_network_behavior',
      'variables': {
        'depends': [
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-iconset-svg/iron-iconset-svg-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-meta/iron-meta-extracted.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../network/cr_onc_types.js',
          'cr_policy_indicator_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr_policy_network_indicator',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../network/cr_onc_types.js',
          'cr_policy_indicator_behavior.js',
          'cr_policy_network_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}

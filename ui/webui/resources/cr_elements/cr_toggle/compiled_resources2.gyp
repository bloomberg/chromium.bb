# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_toggle',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-behaviors/compiled_resources2.gyp:iron-button-state-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-behaviors/compiled_resources2.gyp:paper-ripple-behavior-extracted',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:focus_outline_manager',
        '<(EXTERNS_GYP):pending',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}

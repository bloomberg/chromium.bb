# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'iron-input-extracted',
      'dependencies': [
        '../iron-a11y-announcer/compiled_resources2.gyp:iron-a11y-announcer-extracted',
        '../iron-validatable-behavior/compiled_resources2.gyp:iron-validatable-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}

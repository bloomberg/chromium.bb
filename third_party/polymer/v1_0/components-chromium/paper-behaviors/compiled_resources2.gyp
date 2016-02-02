# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'paper-inky-focus-behavior-extracted',
      'dependencies': [
        '../iron-behaviors/compiled_resources2.gyp:iron-button-state-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-control-state-extracted',
        'paper-ripple-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'paper-ripple-behavior-extracted',
      'dependencies': [
        '../iron-behaviors/compiled_resources2.gyp:iron-button-state-extracted',
        '../paper-ripple/compiled_resources2.gyp:paper-ripple-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}

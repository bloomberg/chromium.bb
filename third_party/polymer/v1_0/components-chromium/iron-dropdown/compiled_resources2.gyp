# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'iron-dropdown-extracted',
      'dependencies': [
        '../iron-behaviors/compiled_resources2.gyp:iron-control-state-extracted',
        '../iron-overlay-behavior/compiled_resources2.gyp:iron-overlay-behavior-extracted',
        '../neon-animation/compiled_resources2.gyp:neon-animation-runner-behavior-extracted',
        'iron-dropdown-scroll-manager-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'iron-dropdown-scroll-manager-extracted',
      'dependencies': [
        '../iron-overlay-behavior/compiled_resources2.gyp:iron-scroll-manager-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}

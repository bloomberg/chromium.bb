# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'command',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
      ],
      'includes': ['../../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'focus_grid',
      'dependencies': [
        '../../compiled_resources2.gyp:assert',
        '../../compiled_resources2.gyp:cr',
        '../../compiled_resources2.gyp:event_tracker',
        'focus_row',
      ],
      'includes': ['../../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'focus_manager',
      'dependencies': ['../../compiled_resources2.gyp:cr'],
      'includes': ['../../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'focus_row',
      'dependencies': [
        '../../compiled_resources2.gyp:assert',
        '../../compiled_resources2.gyp:cr',
        '../../compiled_resources2.gyp:event_tracker',
        '../../compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'overlay',
      'dependencies': [
        '../../compiled_resources2.gyp:cr',
        '../../compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}

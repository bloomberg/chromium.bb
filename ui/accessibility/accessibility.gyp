# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'accessibility',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'ACCESSIBILITY_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under accessibility, except unittests
        'ax_enums.h',
        'ax_node.cc',
        'ax_node_data.cc',
        'ax_node_data.h',
        'ax_node.h',
        'ax_serializable_tree.cc',
        'ax_serializable_tree.h',
        'ax_tree.cc',
        'ax_tree.h',
        'ax_tree_serializer.cc',
        'ax_tree_serializer.h',
        'ax_tree_source.h',
        'ax_tree_update.cc',
        'ax_tree_update.h',
      ]
    },
    {
      'target_name': 'accessibility_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
        'accessibility',
      ],
      'sources': [
        'ax_tree_serializer_unittest.cc',
        'ax_tree_unittest.cc',
      ]
    },
  ],
}
